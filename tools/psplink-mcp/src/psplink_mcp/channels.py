"""TCP clients for usbhostfs_pc's async channels, with automatic reconnect.

usbhostfs_pc listens on 127.0.0.1:<base>+n for each async channel
(usbhostfs/usbhostfs.h): 0 shell, 1 gdb, 2 stdout, 3 stderr, 4+ user (remotejoy).
It accepts a single client per channel and drops the previous one when a new
client connects, so do not run pspsh against the same ports at the same time.
"""

from __future__ import annotations

import asyncio
import logging
from dataclasses import dataclass
from typing import Callable

log = logging.getLogger(__name__)

SHELL = 0
GDB = 1
STDOUT = 2
STDERR = 3
JOY = 4

DEFAULT_BASE_PORT = 10000


@dataclass
class Ports:
    shell: int
    stdout: int
    stderr: int
    joy: int

    @classmethod
    def from_base(cls, base: int = DEFAULT_BASE_PORT) -> "Ports":
        return cls(shell=base + SHELL, stdout=base + STDOUT, stderr=base + STDERR, joy=base + JOY)


class RingBuffer:
    """Bounded byte buffer; the oldest data is dropped when it overflows."""

    def __init__(self, limit: int = 256 * 1024):
        self.limit = limit
        self._buf = bytearray()
        self.dropped = 0

    def write(self, data: bytes) -> None:
        self._buf += data
        overflow = len(self._buf) - self.limit
        if overflow > 0:
            del self._buf[:overflow]
            self.dropped += overflow

    def read(self, clear: bool = True) -> bytes:
        data = bytes(self._buf)
        if clear:
            self._buf.clear()
            self.dropped = 0
        return data

    def clear(self) -> None:
        self._buf.clear()
        self.dropped = 0

    def __len__(self) -> int:
        return len(self._buf)


class Channel:
    """One persistent TCP connection to a usbhostfs_pc channel.

    Received bytes go to `on_data`. The connection is re-established with
    backoff whenever it drops (PSP reset, usbhostfs_pc restart, cable pulled).
    """

    def __init__(self, name: str, host: str, port: int, on_data: Callable[[bytes], None]):
        self.name = name
        self.host = host
        self.port = port
        self.on_data = on_data
        self._writer: asyncio.StreamWriter | None = None
        self._task: asyncio.Task | None = None
        self._connected = asyncio.Event()
        self.on_disconnect: Callable[[], None] | None = None

    @property
    def connected(self) -> bool:
        return self._connected.is_set()

    def start(self) -> None:
        if self._task is None:
            self._task = asyncio.create_task(self._run(), name=f"channel-{self.name}")

    async def stop(self) -> None:
        if self._task:
            self._task.cancel()
            try:
                await self._task
            except asyncio.CancelledError:
                pass
            self._task = None
        self._close_writer()

    async def wait_connected(self, timeout: float) -> bool:
        try:
            await asyncio.wait_for(self._connected.wait(), timeout)
            return True
        except asyncio.TimeoutError:
            return False

    async def write(self, data: bytes) -> None:
        if self._writer is None:
            raise ConnectionError(f"{self.name} channel (port {self.port}) is not connected")
        self._writer.write(data)
        await self._writer.drain()

    def _close_writer(self) -> None:
        if self._writer is not None:
            self._writer.close()
            self._writer = None
        self._connected.clear()

    async def _run(self) -> None:
        delay = 0.5
        while True:
            try:
                reader, writer = await asyncio.open_connection(self.host, self.port)
            except OSError as e:
                log.debug("%s: connect to %s:%d failed: %s", self.name, self.host, self.port, e)
                await asyncio.sleep(delay)
                delay = min(delay * 2, 5.0)
                continue

            log.info("%s: connected to %s:%d", self.name, self.host, self.port)
            self._writer = writer
            self._connected.set()
            delay = 0.5
            try:
                while True:
                    data = await reader.read(4096)
                    if not data:
                        break
                    self.on_data(data)
            except OSError as e:
                log.info("%s: connection error: %s", self.name, e)
            finally:
                log.info("%s: disconnected", self.name)
                self._close_writer()
                if self.on_disconnect:
                    self.on_disconnect()
            await asyncio.sleep(delay)
