"""Convert PSPLink `scrshot` bitmaps (480x272, 24-bit BMP) to PNG."""

from __future__ import annotations

import io

from PIL import Image


def bmp_to_png(data: bytes, scale: float = 1.0) -> bytes:
    with Image.open(io.BytesIO(data)) as img:
        img = img.convert("RGB")
        if scale != 1.0:
            size = (max(1, round(img.width * scale)), max(1, round(img.height * scale)))
            img = img.resize(size, Image.Resampling.LANCZOS)
        out = io.BytesIO()
        img.save(out, format="PNG", optimize=True)
        return out.getvalue()
