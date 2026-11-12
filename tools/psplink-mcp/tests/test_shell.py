import asyncio

import pytest

from psplink_mcp.shell import ShellClient, encode_command


def test_encode_matches_pspsh_wire_format():
    assert encode_command("thlist") == b"thlist\0\x01"
    assert encode_command('peekw 0x08800000 "a b"') == b"peekw\0" + b"0x08800000\0a b\0\x01"


def test_encode_prefixes_ld_for_paths():
    assert encode_command("./app.prx x") == b"ld\0./app.prx\0x\0\x01"
    assert encode_command("host0:/app.elf") == b"ld\0host0:/app.elf\0\x01"


@pytest.mark.parametrize("cmd", ["", "help", "asm 0x0", "set a=b"])
def test_encode_rejects_empty_and_pc_side_commands(cmd):
    with pytest.raises(ValueError):
        encode_command(cmd)


def test_encode_rejects_too_many_args():
    with pytest.raises(ValueError):
        encode_command(" ".join(["a"] * 17))


class Wire:
    def __init__(self):
        self.sent = []
        self.client = ShellClient(self.write)

    async def write(self, data):
        self.sent.append(data)


async def test_result_split_across_chunks():
    w = Wire()
    task = asyncio.create_task(w.client.run("ver", timeout=1))
    await asyncio.sleep(0)
    reply = b"PSPLink v3\r\n\xff\xfb" b"host0:/\xfe\xff\xfd0x00000000\xfe"
    for i in range(0, len(reply), 2):
        w.client.feed(reply[i:i + 2])
    res = await task
    assert res.ok and res.ret == 0 and res.output == "PSPLink v3\n"
    assert w.client.cwd == "host0:/"
    assert w.sent == [b"ver\0\x01"]


async def test_shell_print_text_frames_are_output():
    # Real PSPLink wraps every print in 0xFF ... 0xFE (SHELL_PRINT).
    w = Wire()
    task = asyncio.create_task(w.client.run("modlist", timeout=1))
    await asyncio.sleep(0)
    reply = (b"\xff<Module List (2 modules)>\r\n\xfe"
             b"\xffUID: 0x00000000 Name: PSPLINK\r\n\xfe"
             b"\xff\xfd0x00000000\xfe")
    for i in range(0, len(reply), 3):
        w.client.feed(reply[i:i + 3])
    res = await task
    assert res.ok
    assert res.output == "<Module List (2 modules)>\nUID: 0x00000000 Name: PSPLINK\n"


async def test_framed_text_without_command_is_unsolicited():
    w = Wire()
    w.client.feed(b"\xffException - Bus error (data)\r\n\xfe")
    assert w.client.unsolicited.read() == b"Exception - Bus error (data)\r\n"


async def test_error_frame():
    w = Wire()
    task = asyncio.create_task(w.client.run("modstart 1", timeout=1))
    await asyncio.sleep(0)
    w.client.feed(b"bad\r\n\xff\xfc0x80020001\xfe")
    res = await task
    assert not res.ok and res.ret == 0x80020001


async def test_text_without_command_is_unsolicited():
    w = Wire()
    w.client.feed(b"Exception - Bus error\r\n")
    assert w.client.unsolicited.read() == b"Exception - Bus error\r\n"


async def test_late_reply_after_timeout_is_not_misattributed():
    w = Wire()
    first = await w.client.run("slow", timeout=0.05)
    assert first.timed_out
    task = asyncio.create_task(w.client.run("ver", timeout=1))
    await asyncio.sleep(0)
    w.client.feed(b"slow done\r\n\xff\xfd0x00000000\xfe")  # late answer to "slow"
    w.client.feed(b"v3\r\n\xff\xfd0x00000000\xfe")
    res = await task
    assert res.ok and res.output == "v3\n"
    assert w.client.unsolicited.read() == b"slow done\r\n"


async def test_stale_expectation_expires(monkeypatch):
    import psplink_mcp.shell as shell

    monkeypatch.setattr(shell, "STALE_WINDOW_S", 0.0)
    w = Wire()
    assert (await w.client.run("lost", timeout=0.01)).timed_out  # never answered (PSP rebooted)
    await asyncio.sleep(0.01)
    task = asyncio.create_task(w.client.run("ver", timeout=1))
    await asyncio.sleep(0)
    w.client.feed(b"v3\r\n\xff\xfd0x00000000\xfe")
    assert (await task).ok


async def test_disconnect_fails_pending_command():
    w = Wire()
    task = asyncio.create_task(w.client.run("ver", timeout=5))
    await asyncio.sleep(0)
    w.client.reset()
    with pytest.raises(ConnectionError):
        await task
