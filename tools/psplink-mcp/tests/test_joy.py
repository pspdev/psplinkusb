import struct

import pytest

from psplink_mcp import joy


def test_event_layout_matches_c_struct():
    # struct JoyEvent { unsigned int magic; int type; unsigned int value; } __attribute__((packed))
    data = joy.buttons_event(True, 0x4000)
    assert len(data) == 12
    assert data == bytes.fromhex("EFCC9A90") + struct.pack("<i", 1) + struct.pack("<I", 0x4000)


def test_button_mask_and_aliases():
    assert joy.button_mask(["cross", "UP", "l"]) == 0x4000 | 0x10 | 0x100


def test_unknown_button():
    with pytest.raises(ValueError, match="home"):
        joy.button_mask(["home"])


def test_analog_range():
    assert len(joy.analog_events(0, 255)) == 24
    with pytest.raises(ValueError):
        joy.analog_events(256, 0)
