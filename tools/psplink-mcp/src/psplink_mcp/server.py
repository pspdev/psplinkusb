"""MCP server exposing a PSP running PSPLink as tools for Claude.

Talks to usbhostfs_pc over its local TCP channels (see channels.py), so
usbhostfs_pc must be running with the PSP connected over USB.
"""

from __future__ import annotations

import argparse
import asyncio
import functools
import itertools
import logging
import os
import shlex
import shutil
import sys
from pathlib import Path
from typing import Any

from mcp.server.mcpserver import Image, MCPServer
from mcp.server.mcpserver.exceptions import ToolError

from . import joy
from .channels import DEFAULT_BASE_PORT, Channel, Ports, RingBuffer
from .image import bmp_to_png
from .shell import ShellClient, ShellResult

log = logging.getLogger("psplink_mcp")

WORK_DIR = ".psplink-mcp"
DRIVE_PREFIXES = ("host", "ms0:", "ef0:", "flash", "disc0:", "umd")
# Built by `make -C tools/remotejoy` in this repository.
BUNDLED_REMOTEJOY = Path(__file__).resolve().parents[3] / "remotejoy" / "remotejoy.prx"


class PSP:
    """Connection state and operations; the MCP tools are thin wrappers."""

    def __init__(self, host: str, ports: Ports, root: Path | None, remotejoy: Path | None = None):
        self.host = host
        self.ports = ports
        self.root = root
        self.remotejoy = remotejoy or BUNDLED_REMOTEJOY
        self.stdout = RingBuffer()
        self.stderr = RingBuffer()
        self._shell_chan = Channel("shell", host, ports.shell, self._on_shell_data)
        self.shell = ShellClient(self._shell_chan.write)
        self._shell_chan.on_disconnect = self.shell.reset
        self._chans = [
            self._shell_chan,
            Channel("stdout", host, ports.stdout, self.stdout.write),
            Channel("stderr", host, ports.stderr, self.stderr.write),
            Channel("joy", host, ports.joy, lambda _data: None),
        ]
        self._joy_chan = self._chans[3]
        self._joy_ready = False
        self._shot_ids = itertools.count()
        self._started = False

    def _on_shell_data(self, data: bytes) -> None:
        self.shell.feed(data)

    def start(self) -> None:
        if not self._started:
            for chan in self._chans:
                chan.start()
            self._started = True

    async def stop(self) -> None:
        for chan in self._chans:
            await chan.stop()
        self._started = False

    async def _wait_channels(self, timeout: float) -> None:
        self.start()
        await asyncio.gather(*(c.wait_connected(timeout) for c in self._chans))

    async def _require_shell(self, timeout: float = 3.0) -> None:
        # Waiting for every channel means stdout/stderr are attached before a
        # command runs, so early app output isn't missed.
        await self._wait_channels(timeout)
        if not self._shell_chan.connected:
            raise ConnectionError(
                f"cannot reach usbhostfs_pc on {self.host}:{self.ports.shell}. "
                "Is usbhostfs_pc running, and is nothing else (pspsh) attached to the shell port?"
            )

    async def run_shell(self, command: str, timeout: float = 10.0) -> ShellResult:
        await self._require_shell()
        return await self.shell.run(command, timeout)

    def host_path(self, path: str) -> str:
        """Map a local path or a host0-relative path to a path the PSP can open."""
        if path.lower().startswith(DRIVE_PREFIXES) and ":" in path:
            return path
        p = Path(path).expanduser()
        if p.is_absolute():
            if self.root is None:
                raise ValueError("absolute local paths need --root / PSPLINK_ROOT (the usbhostfs_pc directory)")
            try:
                rel = p.resolve().relative_to(self.root)
            except ValueError:
                raise ValueError(f"{p} is outside the usbhostfs_pc root {self.root}, so the PSP can't see it")
            return "host0:/" + rel.as_posix()
        return "host0:/" + path.lstrip("/")

    def take_output(self, clear: bool = True) -> dict[str, str]:
        out = {}
        for name, buf in (("stdout", self.stdout), ("stderr", self.stderr), ("shell", self.shell.unsolicited)):
            dropped = buf.dropped
            text = buf.read(clear).decode("latin-1", errors="replace").replace("\r\n", "\n")
            if dropped:
                text = f"[... {dropped} earlier bytes dropped ...]\n" + text
            out[name] = text
        return out

    async def status(self) -> dict[str, Any]:
        await self._wait_channels(2.0)
        info: dict[str, Any] = {
            "channels": {c.name: ("connected" if c.connected else "disconnected") for c in self._chans},
            "usbhostfs_root": str(self.root) if self.root else None,
        }
        if not self._shell_chan.connected:
            info["error"] = "shell channel not connected; start usbhostfs_pc and PSPLink"
            return info
        ver = await self.shell.run("ver", timeout=5)
        if ver.timed_out:
            info["error"] = "usbhostfs_pc is up but the PSP did not answer; is PSPLink running and USB connected?"
            return info
        info["psplink"] = ver.output.strip()
        mods = await self.shell.run("modlist", timeout=5)
        info["modules"] = mods.output.strip()
        self._joy_ready = "Name: RemoteJoy" in mods.output
        info["remotejoy_loaded"] = self._joy_ready
        return info

    async def run_app(self, path: str, args: list[str], wait_s: float) -> dict[str, Any]:
        target = self.host_path(path)
        self.take_output(clear=True)
        res = await self.run_shell(" ".join(shlex.quote(a) for a in ["ld", target, *args]), timeout=15)
        await asyncio.sleep(wait_s)
        return {"load": _result_dict(res), **self.take_output(clear=True)}

    async def screenshot(self, scale: float, pri: int) -> bytes:
        if self.root is None:
            raise ValueError("screenshots need --root / PSPLINK_ROOT (the usbhostfs_pc directory)")
        work = self.root / WORK_DIR
        work.mkdir(exist_ok=True)
        name = f"shot-{os.getpid()}-{next(self._shot_ids)}.bmp"
        local = work / name
        res = await self.run_shell(f"scrshot host0:/{WORK_DIR}/{name} {pri}", timeout=15)
        try:
            if not res.ok:
                raise RuntimeError(f"scrshot failed: {res.output.strip() or res}")
            return bmp_to_png(local.read_bytes(), scale)
        finally:
            local.unlink(missing_ok=True)

    async def ensure_remotejoy(self) -> None:
        self.start()
        if not await self._joy_chan.wait_connected(3.0):
            raise ConnectionError(f"cannot reach usbhostfs_pc joy channel on {self.host}:{self.ports.joy}")
        if self._joy_ready:
            return
        mods = await self.run_shell("modlist", timeout=5)
        if "Name: RemoteJoy" not in mods.output:
            await self._load_remotejoy()
        self._joy_ready = True

    async def _load_remotejoy(self) -> None:
        if self.root is None:
            raise ValueError("loading remotejoy.prx needs --root / PSPLINK_ROOT")
        dest = self.root / WORK_DIR / "remotejoy.prx"
        if self.remotejoy.exists():
            dest.parent.mkdir(exist_ok=True)
            if not dest.exists() or dest.stat().st_mtime < self.remotejoy.stat().st_mtime:
                shutil.copy2(self.remotejoy, dest)
        if not dest.exists():
            raise FileNotFoundError(
                f"remotejoy.prx not found. Build it with `make -C tools/remotejoy` or copy it to {dest}"
            )
        res = await self.run_shell(f"ld host0:/{WORK_DIR}/remotejoy.prx", timeout=10)
        if not res.ok:
            raise RuntimeError(f"failed to load remotejoy.prx: {res.output.strip()}")

    async def send_joy(self, data: bytes) -> None:
        await self.ensure_remotejoy()
        await self._joy_chan.write(data)

    async def press(self, buttons: list[str], hold_ms: int) -> None:
        mask = joy.button_mask(buttons)
        await self.send_joy(joy.buttons_event(True, mask))
        await asyncio.sleep(hold_ms / 1000)
        await self.send_joy(joy.buttons_event(False, mask))

    async def analog(self, x: int, y: int, duration_ms: int) -> None:
        await self.send_joy(joy.analog_events(x, y))
        if duration_ms > 0:
            await asyncio.sleep(duration_ms / 1000)
            await self.send_joy(joy.analog_events(joy.ANALOG_CENTRE, joy.ANALOG_CENTRE))

    async def reset(self, timeout_s: float) -> dict[str, Any]:
        await self._require_shell()
        self._joy_ready = False
        # PSPLink reboots before it can answer, so don't wait for a reply.
        await self.shell.run("reset", expect_reply=False)
        self.shell.reset()
        self.shell.prompt_seen.clear()
        loop = asyncio.get_running_loop()
        deadline = loop.time() + timeout_s
        # PSPLink prints a prompt (a bare completion frame) once it is back.
        # Commands sent before that are held by usbhostfs_pc and answered after
        # the prompt, which would then be taken as their result. So wait for it.
        try:
            await asyncio.wait_for(self.shell.prompt_seen.wait(), max(timeout_s - 5, 1))
        except TimeoutError:
            # Missed it (e.g. the shell channel reconnected meanwhile): fall back
            # to polling with whatever time is left.
            deadline = max(deadline, loop.time() + 5)
        while loop.time() < deadline:
            if self._shell_chan.connected:
                try:
                    ver = await self.shell.run("ver", timeout=3)
                    if ver.ok:
                        self.take_output(clear=True)
                        return {"ok": True, "psplink": ver.output.strip()}
                except ConnectionError:
                    pass
            await asyncio.sleep(1)
        return {"ok": False, "error": f"PSP did not come back within {timeout_s}s"}


def _result_dict(res: ShellResult) -> dict[str, Any]:
    d: dict[str, Any] = {"ok": res.ok, "output": res.output}
    if res.ret is not None:
        d["ret"] = f"0x{res.ret:08X}"
    if res.timed_out:
        d["timed_out"] = True
    return d


def _expected_errors(fn):
    """Report anticipated failures (PSP unreachable, bad arguments, ...) to the model.

    MCPServer hides the text of any exception other than ToolError.
    """

    @functools.wraps(fn)
    async def wrapper(*args, **kwargs):
        try:
            return await fn(*args, **kwargs)
        except (OSError, ValueError, RuntimeError) as e:
            raise ToolError(str(e)) from e

    return wrapper


def build_server(psp: PSP) -> MCPServer:
    mcp = MCPServer(
        "psplink",
        instructions=(
            "Controls a real Sony PSP running PSPLink over USB. Start with psp_status. "
            "The usbhostfs_pc directory is the PSP's host0:, so build apps into it, then psp_run them. "
            "Use psp_screenshot to see the screen and psp_press / psp_input_sequence to play. "
            "To re-run an app or recover from a crash, call psp_reset first."
        ),
    )

    @mcp.tool()
    @_expected_errors
    async def psp_status() -> dict[str, Any]:
        """Connection state, PSPLink version, loaded modules and whether remotejoy (input) is loaded."""
        return await psp.status()

    @mcp.tool()
    @_expected_errors
    async def psp_shell(command: str, timeout_s: float = 10.0) -> dict[str, Any]:
        """Run one PSPLink shell command on the PSP and return its output.

        Examples: "thlist", "modlist", "meminfo", "peekw 0x08800000", "exprint", "ls host0:/".
        pspsh-only commands (asm, help, set, echo, symload...) are not available.
        """
        return _result_dict(await psp.run_shell(command, timeout_s))

    @mcp.tool()
    @_expected_errors
    async def psp_run(path: str, args: list[str] | None = None, wait_s: float = 3.0) -> dict[str, Any]:
        """Load and start a .prx or .elf on the PSP, then collect its output for wait_s seconds.

        path is relative to the usbhostfs_pc directory (host0:), an absolute local path
        inside it, or a full PSP path like "ms0:/PSP/GAME/foo/EBOOT.PBP".
        Returns the load result plus stdout, stderr and any unsolicited shell text
        (exception reports appear there). Call psp_reset before running an app again.
        """
        return await psp.run_app(path, args or [], wait_s)

    @mcp.tool()
    @_expected_errors
    async def psp_output(clear: bool = True) -> dict[str, str]:
        """Return stdout, stderr and unsolicited shell text (e.g. exceptions) buffered since the last read."""
        return psp.take_output(clear)

    @mcp.tool()
    @_expected_errors
    async def psp_screenshot(scale: float = 1.0, pri: int = 2) -> Image:
        """Capture the PSP screen (480x272) as a PNG. scale resizes it (e.g. 0.5).

        pri is the framebuffer priority passed to scrshot; the default 2 is the
        normal game framebuffer.
        """
        return Image(data=await psp.screenshot(scale, pri), format="png")

    @mcp.tool()
    @_expected_errors
    async def psp_press(buttons: list[str], hold_ms: int = 100) -> str:
        """Press and release buttons together (a chord), holding them for hold_ms.

        Buttons: cross, circle, square, triangle, up, down, left, right,
        ltrigger, rtrigger, start, select. HOME and NOTE are not supported.
        """
        await psp.press(buttons, hold_ms)
        return f"pressed {'+'.join(buttons)} for {hold_ms} ms"

    @mcp.tool()
    @_expected_errors
    async def psp_buttons(down: list[str] | None = None, up: list[str] | None = None) -> str:
        """Hold buttons down and/or release them, without timing. Use to keep a button held across calls."""
        if down:
            await psp.send_joy(joy.buttons_event(True, joy.button_mask(down)))
        if up:
            await psp.send_joy(joy.buttons_event(False, joy.button_mask(up)))
        return f"down={down or []} up={up or []}"

    @mcp.tool()
    @_expected_errors
    async def psp_analog(x: int, y: int, duration_ms: int = 300) -> str:
        """Move the analog stick to (x, y), each 0-255 with 128 as centre (0,0 = top left).

        After duration_ms the stick is recentred; pass 0 to leave it where it is.
        """
        await psp.analog(x, y, duration_ms)
        return f"analog ({x}, {y}) for {duration_ms} ms"

    @mcp.tool()
    @_expected_errors
    async def psp_input_sequence(steps: list[dict[str, Any]]) -> str:
        """Run several inputs in one call.

        Each step is one of:
          {"press": ["cross"], "hold_ms": 100}
          {"wait_ms": 500}
          {"analog": [x, y], "duration_ms": 300}
        """
        for i, step in enumerate(steps):
            if "press" in step:
                await psp.press(step["press"], int(step.get("hold_ms", 100)))
            elif "wait_ms" in step:
                await asyncio.sleep(int(step["wait_ms"]) / 1000)
            elif "analog" in step:
                x, y = step["analog"]
                await psp.analog(int(x), int(y), int(step.get("duration_ms", 300)))
            else:
                raise ValueError(f"step {i}: expected 'press', 'wait_ms' or 'analog', got {step}")
        return f"ran {len(steps)} steps"

    @mcp.tool()
    @_expected_errors
    async def psp_reset(timeout_s: float = 30.0) -> dict[str, Any]:
        """Reset PSPLink (unloads the running app) and wait until the PSP answers again."""
        return await psp.reset(timeout_s)

    return mcp


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default=os.environ.get("PSPLINK_HOST", "127.0.0.1"))
    parser.add_argument("--base-port", type=int, default=int(os.environ.get("PSPLINK_BASE_PORT", DEFAULT_BASE_PORT)))
    parser.add_argument("--root", default=os.environ.get("PSPLINK_ROOT"),
                        help="directory usbhostfs_pc serves as host0: (needed for screenshots and remotejoy)")
    parser.add_argument("--remotejoy", default=os.environ.get("PSPLINK_REMOTEJOY"),
                        help=f"path to remotejoy.prx (default: {BUNDLED_REMOTEJOY})")
    parser.add_argument("-v", "--verbose", action="store_true")
    args = parser.parse_args()

    # stdout carries the MCP protocol; logs must go to stderr.
    logging.basicConfig(stream=sys.stderr, level=logging.DEBUG if args.verbose else logging.INFO,
                        format="%(asctime)s %(name)s %(levelname)s %(message)s")
    root = Path(args.root).expanduser().resolve() if args.root else None  # "" (unset in .mcp.json) = no root
    psp = PSP(args.host, Ports.from_base(args.base_port), root,
              Path(args.remotejoy).expanduser() if args.remotejoy else None)
    build_server(psp).run()


if __name__ == "__main__":
    main()
