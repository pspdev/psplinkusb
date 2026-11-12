import asyncio
import io

import pytest
from PIL import Image

from psplink_mcp import joy
from psplink_mcp.channels import Ports
from psplink_mcp.server import PSP, build_server

from .fake_usbhostfs import FakePSP


@pytest.fixture
async def rig(tmp_path):
    remotejoy = tmp_path / "build" / "remotejoy.prx"
    remotejoy.parent.mkdir()
    remotejoy.write_bytes(b"~PSP fake prx")
    root = tmp_path / "host0"
    root.mkdir()
    fake = FakePSP(root)
    ports = await fake.start()
    psp = PSP("127.0.0.1", ports, root, remotejoy)
    yield fake, psp, root
    await psp.stop()
    await fake.stop()


async def test_status(rig):
    fake, psp, _ = rig
    st = await psp.status()
    assert st["psplink"] == "PSPLink v3.3.0"
    assert st["remotejoy_loaded"] is False
    assert all(v == "connected" for v in st["channels"].values())


async def test_status_without_usbhostfs(tmp_path):
    psp = PSP("127.0.0.1", Ports.from_base(1), tmp_path)
    st = await psp.status()
    assert "error" in st
    await psp.stop()


async def test_run_collects_stdout_and_exceptions(rig):
    fake, psp, root = rig
    res = await psp.run_app(str(root / "games" / "crash.prx"), ["--fast"], wait_s=0.3)
    assert fake.commands[-1] == ["ld", "host0:/games/crash.prx", "--fast"]
    assert res["load"]["ok"] and res["load"]["ret"] == "0x04A0F2B3"
    assert "hello from host0:/games/crash.prx args=['--fast']" in res["stdout"]
    assert "Bus error" in res["shell"]


def test_host_path(tmp_path):
    psp = PSP("127.0.0.1", Ports.from_base(), tmp_path)
    assert psp.host_path("app.prx") == "host0:/app.prx"
    assert psp.host_path("/x/../app.prx".replace("/x/..", str(tmp_path))) == "host0:/app.prx"
    assert psp.host_path("ms0:/PSP/GAME/a/EBOOT.PBP") == "ms0:/PSP/GAME/a/EBOOT.PBP"
    with pytest.raises(ValueError):
        psp.host_path("/etc/passwd")


async def test_shell_error(rig):
    _, psp, _ = rig
    res = await psp.run_shell("fail")
    assert not res.ok and res.ret == 0x80020001


async def test_screenshot_png(rig):
    _, psp, root = rig
    png = await psp.screenshot(scale=0.5, pri=2)
    img = Image.open(io.BytesIO(png))
    assert img.format == "PNG" and img.size == (240, 136)
    assert not list((root / ".psplink-mcp").glob("*.bmp"))


async def test_press_autoloads_remotejoy(rig):
    fake, psp, root = rig
    await psp.press(["cross", "up"], hold_ms=10)
    await asyncio.sleep(0.05)
    assert ["ld", "host0:/.psplink-mcp/remotejoy.prx"] in fake.commands
    assert (root / ".psplink-mcp" / "remotejoy.prx").exists()
    mask = joy.BUTTONS["cross"] | joy.BUTTONS["up"]
    assert fake.joy_events == [
        (joy.JOY_MAGIC, joy.TYPE_BUTTON_DOWN, mask),
        (joy.JOY_MAGIC, joy.TYPE_BUTTON_UP, mask),
    ]


async def test_analog_recentres(rig):
    fake, psp, _ = rig
    await psp.analog(0, 255, duration_ms=10)
    await asyncio.sleep(0.05)
    assert [(t, v) for _, t, v in fake.joy_events] == [
        (joy.TYPE_ANALOG_X, 0), (joy.TYPE_ANALOG_Y, 255),
        (joy.TYPE_ANALOG_X, 128), (joy.TYPE_ANALOG_Y, 128),
    ]


async def test_reset_waits_for_prompt_and_reloads_remotejoy(rig):
    fake, psp, _ = rig
    # A real PSP takes a few seconds to come back, so the commands sent while
    # it boots are answered after its unprompted boot prompt.
    fake.boot_s = 2.5
    await psp.press(["start"], hold_ms=1)
    res = await psp.reset(timeout_s=15)
    assert res == {"ok": True, "psplink": "PSPLink v3.3.0"}
    assert not fake.remotejoy_loaded
    await psp.press(["start"], hold_ms=1)
    assert fake.remotejoy_loaded


async def test_reconnects_after_usbhostfs_drops_clients(rig):
    fake, psp, _ = rig
    assert (await psp.status())["psplink"] == "PSPLink v3.3.0"
    fake.drop_clients()
    for _ in range(50):
        await asyncio.sleep(0.1)
        if psp._shell_chan.connected:
            break
    assert (await psp.status())["psplink"] == "PSPLink v3.3.0"


async def test_tools_are_registered(rig):
    _, psp, _ = rig
    tools = {t.name for t in await build_server(psp).list_tools()}
    assert tools == {
        "psp_status", "psp_shell", "psp_run", "psp_output", "psp_screenshot", "psp_press",
        "psp_buttons", "psp_analog", "psp_input_sequence", "psp_reset",
    }


async def test_tool_schema_keeps_parameters(rig):
    _, psp, _ = rig
    tools = {t.name: t for t in await build_server(psp).list_tools()}
    assert set(tools["psp_press"].input_schema["properties"]) == {"buttons", "hold_ms"}
    assert tools["psp_press"].input_schema["required"] == ["buttons"]


async def test_expected_errors_reach_the_model(rig):
    from mcp.server.mcpserver.exceptions import ToolError

    _, psp, _ = rig
    server = build_server(psp)
    with pytest.raises(ToolError, match="unknown button 'home'"):
        await server.call_tool("psp_press", {"buttons": ["home"]})
    with pytest.raises(ToolError, match="pspsh"):
        await server.call_tool("psp_shell", {"command": "help"})
