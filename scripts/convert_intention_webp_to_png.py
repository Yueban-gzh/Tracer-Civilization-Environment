# -*- coding: utf-8 -*-
"""
灰机维基等来源常把 WebP 存成 .png 扩展名，SFML 按 PNG 解码会报 corrupt。
将 assets/intention/*.png 中实为 WebP 的文件转为真 PNG（覆盖原文件）。

依赖: pip install Pillow
用法: python scripts/convert_intention_webp_to_png.py
"""
from __future__ import annotations

import glob
import os
import sys

try:
    from PIL import Image
except ImportError:
    print("请先安装: pip install Pillow", file=sys.stderr)
    sys.exit(1)


def is_riff_webp(path: str) -> bool:
    with open(path, "rb") as f:
        sig = f.read(12)
    return len(sig) >= 12 and sig[:4] == b"RIFF" and sig[8:12] == b"WEBP"


def main() -> None:
    root = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", "assets", "intention"))
    n = 0
    for path in glob.glob(os.path.join(root, "*.png")):
        if not is_riff_webp(path):
            continue
        im = Image.open(path)
        im.load()
        if im.mode == "RGB":
            pass
        elif im.mode == "RGBA":
            pass
        else:
            im = im.convert("RGBA")
        tmp = path + ".tmpconv.png"
        im.save(tmp, "PNG", optimize=True)
        os.replace(tmp, path)
        print("converted:", os.path.basename(path))
        n += 1
    print("total webp->png:", n)


if __name__ == "__main__":
    main()
