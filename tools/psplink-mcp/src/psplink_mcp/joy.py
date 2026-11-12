"""remotejoy input injection (tools/remotejoy).

remotejoy.prx hooks sceCtrlRead/Peek on the PSP and ORs in the buttons it
receives on async channel 4 as packed little-endian
struct JoyEvent { u32 magic; i32 type; u32 value } (tools/remotejoy/remotejoy.h).
"""

from __future__ import annotations

import struct

JOY_MAGIC = 0x909ACCEF

TYPE_BUTTON_DOWN = 1
TYPE_BUTTON_UP = 2
TYPE_ANALOG_Y = 3
TYPE_ANALOG_X = 4
TYPE_SCREEN_CMD = 5

ANALOG_CENTRE = 0x80

# PSP_CTRL_* masks (pspctrl.h). HOME/NOTE/SCREEN/VOL are not delivered by
# remotejoy because the system reads them through other paths.
BUTTONS = {
    "select": 0x000001,
    "start": 0x000008,
    "up": 0x000010,
    "right": 0x000020,
    "down": 0x000040,
    "left": 0x000080,
    "ltrigger": 0x000100,
    "rtrigger": 0x000200,
    "triangle": 0x001000,
    "circle": 0x002000,
    "cross": 0x004000,
    "square": 0x008000,
}

ALIASES = {
    "l": "ltrigger",
    "r": "rtrigger",
    "x": "cross",
    "o": "circle",
    "tri": "triangle",
    "sq": "square",
}


def button_mask(names: list[str]) -> int:
    mask = 0
    for raw in names:
        name = ALIASES.get(raw.strip().lower(), raw.strip().lower())
        if name not in BUTTONS:
            valid = ", ".join(sorted(BUTTONS))
            raise ValueError(f"unknown button '{raw}' (valid: {valid})")
        mask |= BUTTONS[name]
    return mask


def pack_event(type_: int, value: int) -> bytes:
    return struct.pack("<IiI", JOY_MAGIC, type_, value)


def buttons_event(down: bool, mask: int) -> bytes:
    return pack_event(TYPE_BUTTON_DOWN if down else TYPE_BUTTON_UP, mask)


def analog_events(x: int, y: int) -> bytes:
    for v in (x, y):
        if not 0 <= v <= 255:
            raise ValueError("analog values must be 0-255 (128 = centre)")
    return pack_event(TYPE_ANALOG_X, x) + pack_event(TYPE_ANALOG_Y, y)
