"""A stand-in for usbhostfs_pc + PSPLink that speaks the real wire formats.

Each channel listens on an ephemeral port. The shell decodes NUL-separated
arguments ending in 0x01 and answers like PSPLink: one 0xFF <text> 0xFE frame
per printed line, then a 0xFF <code> <ret> 0xFE completion frame, sent in small
chunks to exercise the client's reassembly.
"""

from __future__ import annotations

import asyncio
import io
import struct
from pathlib import Path

from PIL import Image

from psplink_mcp.channels import Ports

SUCCESS = 0xFD
ERROR = 0xFC
CWD = 0xFB


def frame(code: int, payload: str) -> bytes:
    return bytes([0xFF, code]) + payload.encode() + bytes([0xFE])


def text_frames(text: str) -> bytes:
    """Text the way PSPLink sends it: one SHELL_PRINT frame per line."""
    lines = text.replace("\n", "\r\n").splitlines(keepends=True)
    return b"".join(b"\xff" + line.encode() + b"\xfe" for line in lines)


class FakePSP:
    def __init__(self, root: Path):
        self.root = root
        self.commands: list[list[str]] = []
        self.joy_events: list[tuple[int, int, int]] = []
        self.remotejoy_loaded = False
        self.rebooting = False
        self._held: list[list[str]] = []
        self.boot_s = 0.5  # how long a reset keeps PSPLink away
        self._servers: list[asyncio.base_events.Server] = []
        self._clients: dict[str, asyncio.StreamWriter] = {}

    async def start(self, base_port: int | None = None) -> Ports:
        """Listen on ephemeral ports, or on usbhostfs_pc's layout from base_port."""
        fixed = Ports.from_base(base_port) if base_port else None
        ports = {}
        for name, handler in (("shell", self._shell), ("stdout", self._sink), ("stderr", self._sink), ("joy", self._joy)):
            port = getattr(fixed, name) if fixed else 0
            server = await asyncio.start_server(self._wrap(name, handler), "127.0.0.1", port)
            self._servers.append(server)
            ports[name] = server.sockets[0].getsockname()[1]
        return Ports(**ports)

    async def stop(self) -> None:
        self.drop_clients()
        for s in self._servers:
            s.close()
            await s.wait_closed()

    def drop_clients(self) -> None:
        for w in self._clients.values():
            w.close()
        self._clients.clear()

    def _wrap(self, name, handler):
        async def run(reader, writer):
            old = self._clients.get(name)
            if old:
                old.close()  # usbhostfs_pc keeps one client per channel
            self._clients[name] = writer
            try:
                await handler(reader, writer)
            except (ConnectionError, asyncio.IncompleteReadError):
                pass
        return run

    async def _send(self, name: str, data: bytes, chunk: int = 3) -> None:
        w = self._clients.get(name)
        if w is None:
            return
        for i in range(0, len(data), chunk):
            w.write(data[i:i + chunk])
            await w.drain()

    async def _sink(self, reader, writer):
        while await reader.read(1024):
            pass

    async def _joy(self, reader, writer):
        while True:
            data = await reader.readexactly(12)
            self.joy_events.append(struct.unpack("<IiI", data))

    async def _shell(self, reader, writer):
        while True:
            raw = await reader.readuntil(b"\x01")
            args = raw[:-1].split(b"\0")[:-1]
            argv = [a.decode() for a in args]
            self.commands.append(argv)
            if self.rebooting:
                # USB is down; usbhostfs_pc holds the bytes until PSPLink is back.
                self._held.append(argv)
                continue
            await self._dispatch(argv)

    async def _reply(self, text: str, ok: bool = True, ret: int = 0) -> None:
        await self._send("shell", text_frames(text) + frame(SUCCESS if ok else ERROR, f"0x{ret:08X}"))

    async def _dispatch(self, argv: list[str]) -> None:
        cmd, args = argv[0], argv[1:]
        if cmd == "ver":
            await self._reply("PSPLink v3.3.0\n")
        elif cmd == "modlist":
            mods = ["sceSystemMemoryManager", "PSPLINK"] + (["RemoteJoy"] if self.remotejoy_loaded else [])
            lines = [f"<Module List ({len(mods)} modules)>"]
            lines += [f"UID: 0x0000{i:04X} Attr: 1007 - Name: {m}" for i, m in enumerate(mods)]
            await self._reply("\n".join(lines) + "\n")
        elif cmd == "ld":
            path = args[0]
            if path.endswith("remotejoy.prx"):
                local = self.root / path.removeprefix("host0:/")
                if not local.exists():
                    await self._reply(f"Failed to Load/Start module '{path}' Error: 0x80010002\n", ok=False)
                    return
                self.remotejoy_loaded = True
            await self._reply(f"Load/Start {path} UID: 0x04A0F2B3 Name: app\n", ret=0x04A0F2B3)
            await self._send("stdout", f"hello from {path} args={args[1:]}\n".encode())
            if "crash" in path:
                await asyncio.sleep(0.05)
                await self._send("shell", text_frames("Exception - Bus error (data)\nThread ID - 0x04A1C0D3\n"))
        elif cmd == "scrshot":
            local = self.root / args[0].removeprefix("host0:/")
            buf = io.BytesIO()
            Image.new("RGB", (480, 272), (200, 30, 30)).save(buf, format="BMP")
            local.write_bytes(buf.getvalue())
            await self._reply(f"frame_addr 0x44000000, frame_width 512, pixel_format 3 output {args[0]}\n")
        elif cmd == "fail":
            await self._reply("ERROR: nope\n", ok=False, ret=0x80020001)
        elif cmd == "slow":
            await asyncio.sleep(float(args[0]))
            await self._reply("slow done\n")
        elif cmd == "reset":
            asyncio.get_running_loop().create_task(self._reboot())
        else:
            await self._reply(f"Unknown command {cmd}\n", ok=False)

    async def _reboot(self) -> None:
        # usbhostfs_pc keeps its TCP clients while the PSP reboots; only USB drops.
        self.rebooting = True
        self.remotejoy_loaded = False
        await asyncio.sleep(self.boot_s)
        self.rebooting = False
        # PSPLink announces itself unprompted (usbShellInit -> psplinkPrintPrompt)
        # before it reads any command that was sent while it was down.
        await self._send("shell", frame(CWD, "host0:/") + frame(SUCCESS, "0x00000000"))
        await asyncio.sleep(0.1)  # the shell thread then starts reading commands
        held, self._held = self._held, []
        for argv in held:
            await self._dispatch(argv)
