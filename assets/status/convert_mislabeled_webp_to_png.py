#!/usr/bin/env python3
"""将扩展名为 .png 但实际为 RIFF/WEBP 的图标转为真 PNG，供 SFML 加载。"""
from __future__ import annotations

import os
import tempfile

from PIL import Image

def is_webp_riff(path: str) -> bool:
    try:
        with open(path, "rb") as f:
            h = f.read(12)
    except OSError:
        return False
    return len(h) >= 12 and h[:4] == b"RIFF" and h[8:12] == b"WEBP"


def main() -> int:
    here = os.path.dirname(os.path.abspath(__file__))
    n = 0
    for name in os.listdir(here):
        if not name.lower().endswith(".png"):
            continue
        path = os.path.join(here, name)
        if not os.path.isfile(path) or not is_webp_riff(path):
            continue
        img = Image.open(path)
        if img.mode not in ("RGB", "RGBA"):
            img = img.convert("RGBA")
        fd, tmp = tempfile.mkstemp(suffix=".png", dir=here)
        os.close(fd)
        try:
            img.save(tmp, "PNG", optimize=True)
            os.replace(tmp, path)
            n += 1
        except Exception:
            try:
                os.unlink(tmp)
            except OSError:
                pass
            raise
    print("done,", n, "files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
