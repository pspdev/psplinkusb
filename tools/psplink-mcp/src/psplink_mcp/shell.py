"""PSPLink shell protocol (channel 0).

Request (see pspsh/parse_args.cpp parse_cli and psplink/usbshell.c
usbShellReadInput): every argument NUL-terminated, then a single 0x01 byte.

Response: a stream of frames 0xFF <payload> 0xFE. PSPLink prints all of its
text through SHELL_PRINT (psplink/psplink.h), which wraps each print in a
frame, so a frame whose first byte is below 0x80 is plain text ("\\r\\n" line
endings). Frames whose first byte is >= 0x80 are control codes
(SHELL_PRINT_CMD, psplink/shellcmd.h); pspsh/pspsh.cpp process_cmd tells them
apart the same way. Each command finishes with a SUCCESS (0xFD) or ERROR (0xFC)
frame whose payload is the command's return value formatted as "0x%08X"
(psplink/shell.c). Bytes outside any frame are kept as text too.
"""

from __future__ import annotations

import asyncio
import shlex
from dataclasses import dataclass

from .channels import RingBuffer

CMD_BEGIN = 0xFF
CMD_END = 0xFE
CMD_SUCCESS = 0xFD
CMD_ERROR = 0xFC
CMD_CWD = 0xFB

MAX_ARGS = 16
STALE_WINDOW_S = 30.0
MAX_CLI = 4096

# Commands that need pspsh's PC-side handling (SHELL_CMD_PCTERM and
# SHELL_CMD_SHARED in psplink/shellcmd.h), so they can't be sent raw.
HOST_ONLY_COMMANDS = {
    "asm", "disopts", "close", "tty", "env", "set", "unset", "echo", "error",
    "strlen", "help", "?", "symload", "exit", "quit",
}


@dataclass
class ShellResult:
    ok: bool
    ret: int | None
    output: str
    timed_out: bool = False


def encode_command(command: str) -> bytes:
    args = shlex.split(command)
    if not args:
        raise ValueError("empty command")
    if args[0] in HOST_ONLY_COMMANDS:
        raise ValueError(f"'{args[0]}' is a pspsh (PC-side) command and is not available here")
    # Same shortcut as pspsh: a path-like first word means "load and start it".
    if "." in args[0] or "/" in args[0]:
        args.insert(0, "ld")
    if len(args) > MAX_ARGS:
        raise ValueError(f"too many arguments ({len(args)} > {MAX_ARGS})")
    data = b"".join(a.encode("latin-1") + b"\0" for a in args) + b"\x01"
    if len(data) > MAX_CLI:
        raise ValueError("command too long")
    return data


def _parse_ret(payload: bytes) -> int | None:
    try:
        return int(payload.decode("latin-1").strip(), 16)
    except ValueError:
        return None


class ShellClient:
    """Frame parser plus one-command-at-a-time request/response handling."""

    def __init__(self, write):
        self._write = write
        self._lock = asyncio.Lock()
        self._in_frame = False
        self._frame = bytearray()
        self._pending: asyncio.Future | None = None
        self._output = bytearray()
        # Completions still owed by commands we gave up waiting for. They are
        # only expected for a while: a command lost in a PSP reboot never answers.
        self._stale = 0
        self._stale_deadline = 0.0
        self.cwd: str | None = None
        # Shell text that arrived with no command in flight (exception dumps, etc.)
        self.unsolicited = RingBuffer()
        # Set by a completion frame no command was waiting for. After a reboot
        # that is PSPLink's own prompt (usbShellInit -> psplinkPrintPrompt).
        self.prompt_seen = asyncio.Event()

    def feed(self, data: bytes) -> None:
        text = bytearray()
        for b in data:
            if b == CMD_BEGIN:
                self._flush_text(text)
                self._in_frame = True
                self._frame.clear()
            elif b == CMD_END and self._in_frame:
                self._in_frame = False
                self._handle_frame(bytes(self._frame))
            elif self._in_frame:
                self._frame.append(b)
            else:
                text.append(b)
        self._flush_text(text)

    def reset(self) -> None:
        """Forget parser state after the connection drops."""
        self._in_frame = False
        self._frame.clear()
        self._stale = 0
        if self._pending and not self._pending.done():
            self._pending.set_exception(ConnectionError("shell connection lost"))

    def _flush_text(self, text: bytearray) -> None:
        if not text:
            return
        if self._pending is not None and self._stale == 0:
            self._output += text
        else:
            self.unsolicited.write(bytes(text))
        text.clear()

    def _handle_frame(self, frame: bytes) -> None:
        if not frame:
            return
        code, payload = frame[0], frame[1:]
        if code < 0x80:
            self._flush_text(bytearray(frame))  # a SHELL_PRINT text frame
        elif code == CMD_CWD:
            self.cwd = payload.decode("latin-1")
        elif code in (CMD_SUCCESS, CMD_ERROR):
            if self._stale > 0:
                self._stale -= 1
                return
            if self._pending is not None and not self._pending.done():
                self._pending.set_result((code == CMD_SUCCESS, _parse_ret(payload)))
            else:
                self.prompt_seen.set()
        # Other frames (tab completion, disasm, symload) are pspsh UI helpers.

    async def run(self, command: str, timeout: float = 10.0, expect_reply: bool = True) -> ShellResult:
        data = encode_command(command)
        async with self._lock:
            loop = asyncio.get_running_loop()
            if self._stale and loop.time() > self._stale_deadline:
                self._stale = 0
            self._output.clear()
            self._pending = loop.create_future()
            try:
                await self._write(data)
                if not expect_reply:
                    return ShellResult(ok=True, ret=None, output="")
                try:
                    ok, ret = await asyncio.wait_for(asyncio.shield(self._pending), timeout)
                    timed_out = False
                except asyncio.TimeoutError:
                    ok, ret, timed_out = False, None, True
                    self._stale += 1
                    self._stale_deadline = loop.time() + STALE_WINDOW_S
                output = self._output.decode("latin-1").replace("\r\n", "\n")
                return ShellResult(ok=ok, ret=ret, output=output, timed_out=timed_out)
            finally:
                self._pending = None
                self._output.clear()
