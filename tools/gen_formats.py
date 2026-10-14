#!/usr/bin/env python3
"""Generates the content-pipeline sample files (needs Pillow):

  Content/Formats/sample.{png,jpg,bmp,tga,gif,tif,qoi}   one picture in seven loose-file formats
  Content/Formats/sample.dds                              the same picture as DXT1 with a full mip chain (Texture2D.DDSFromStreamEXT)
  Content/Formats/broken.png                              a PNG cut off in the middle (the failure exhibit)
  Content/Formats/notes.txt                               a text file for TitleContainer.OpenStream
  Content/Custom/{soup.recipe,stew.cnj,mystery.cnj}      sample assets of the custom-type room (hand-written, tiny)
  Content/Pipeline/{picture.png,sound.wav,shape.gltf}     sources the build runs through cna-content

Run from the repository root:  python3 tools/gen_formats.py
"""
import os
import shutil
import struct

from PIL import Image, ImageDraw

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Content")
W, H = 192, 128


def picture():
    img = Image.new("RGB", (W, H))
    px = img.load()
    for y in range(H):
        for x in range(W):
            px[x, y] = (int(255 * x / (W - 1)), int(255 * y / (H - 1)), 140 + int(60 * ((x // 16 + y // 16) % 2)))
    d = ImageDraw.Draw(img)
    d.ellipse((60, 24, 132, 96), outline=(255, 255, 255), width=4)
    d.rectangle((84, 48, 108, 72), fill=(20, 24, 40))
    d.line((0, 0, W, H), fill=(255, 220, 80), width=3)
    d.text((8, 108), "CNA formats", fill=(255, 255, 255))
    return img


def write_qoi(img, path):
    """A valid QOI file using only the literal RGB(A) opcodes: simple, and every decoder reads it."""
    rgba = img.convert("RGBA")
    out = bytearray(b"qoif" + struct.pack(">IIBB", rgba.width, rgba.height, 4, 0))
    raw = rgba.tobytes()
    for i in range(0, len(raw), 4):
        out += b"\xff" + raw[i:i + 4]
    out += b"\x00\x00\x00\x00\x00\x00\x00\x01"
    with open(path, "wb") as f:
        f.write(out)


def rgb565(c):
    return ((c[0] >> 3) << 11) | ((c[1] >> 2) << 5) | (c[2] >> 3)


def expand565(v):
    r, g, b = (v >> 11) & 31, (v >> 5) & 63, v & 31
    return ((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2))


def dxt1_level(img):
    """DXT1 blocks for one mip level: the two extreme colours of each 4x4 block (by brightness) and the nearest of four palette entries per texel."""
    w, h = img.size
    px = img.convert("RGB").load()
    out = bytearray()
    for by in range((h + 3) // 4):
        for bx in range((w + 3) // 4):
            block = [px[min(bx * 4 + x, w - 1), min(by * 4 + y, h - 1)] for y in range(4) for x in range(4)]
            hi = max(block, key=lambda c: c[0] * 3 + c[1] * 6 + c[2])
            lo = min(block, key=lambda c: c[0] * 3 + c[1] * 6 + c[2])
            c0, c1 = rgb565(hi), rgb565(lo)
            if c0 < c1:
                c0, c1 = c1, c0
            if c0 == c1:
                c0 = min(c0 + 1, 0xFFFF)
            e0, e1 = expand565(c0), expand565(c1)
            palette = [e0, e1, tuple((2 * a + b) // 3 for a, b in zip(e0, e1)), tuple((a + 2 * b) // 3 for a, b in zip(e0, e1))]
            bits = 0
            for i, c in enumerate(block):
                best = min(range(4), key=lambda k: sum((a - b) ** 2 for a, b in zip(c, palette[k])))
                bits |= best << (2 * i)
            out += struct.pack("<HHI", c0, c1, bits)
    return bytes(out)


def write_dds(img, path):
    """A DDS file with DXT1 and a mip chain down to 1 x 1: the compressed texture format games ship."""
    levels = []
    cur = img.convert("RGB")
    while True:
        levels.append(cur)
        if cur.width == 1 and cur.height == 1:
            break
        cur = cur.resize((max(1, cur.width // 2), max(1, cur.height // 2)), Image.LANCZOS)
    data = [dxt1_level(l) for l in levels]
    header = struct.pack("<4sI", b"DDS ", 124) + struct.pack("<7I", 0x1 | 0x2 | 0x4 | 0x1000 | 0x20000 | 0x80000, img.height, img.width, len(data[0]), 0, len(levels), 0)
    header += b"\0" * 40 + struct.pack("<II4s5I", 32, 0x4, b"DXT1", 0, 0, 0, 0, 0) + struct.pack("<5I", 0x1000 | 0x400000 | 0x8, 0, 0, 0, 0)
    assert len(header) == 128, len(header)
    with open(path, "wb") as f:
        f.write(header + b"".join(data))


def main():
    fmt = os.path.join(ROOT, "Formats")
    pipe = os.path.join(ROOT, "Pipeline")
    os.makedirs(fmt, exist_ok=True)
    os.makedirs(pipe, exist_ok=True)
    img = picture()
    img.save(os.path.join(fmt, "sample.png"))
    img.save(os.path.join(fmt, "sample.jpg"), quality=88)
    img.save(os.path.join(fmt, "sample.bmp"))
    img.save(os.path.join(fmt, "sample.tga"))
    img.convert("P", palette=Image.ADAPTIVE).save(os.path.join(fmt, "sample.gif"))
    img.save(os.path.join(fmt, "sample.tif"))
    write_qoi(img, os.path.join(fmt, "sample.qoi"))
    write_dds(img, os.path.join(fmt, "sample.dds"))
    with open(os.path.join(fmt, "sample.png"), "rb") as f:
        data = f.read()
    with open(os.path.join(fmt, "broken.png"), "wb") as f:
        f.write(data[: len(data) // 2])
    with open(os.path.join(fmt, "notes.txt"), "w") as f:
        f.write("This file was read with TitleContainer.OpenStream.\nIt is shipped beside the executable, not compiled.\n")
    big = Image.new("RGB", (256, 256))
    big.paste(img.resize((256, 256)))
    big.save(os.path.join(pipe, "picture.png"))
    shutil.copy(os.path.join(ROOT, "Audio", "chirp.wav"), os.path.join(pipe, "sound.wav"))
    shutil.copy(os.path.join(ROOT, "Models", "cube.gltf"), os.path.join(pipe, "shape.gltf"))
    print("wrote", fmt, "and", pipe)


if __name__ == "__main__":
    main()
