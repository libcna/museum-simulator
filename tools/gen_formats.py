#!/usr/bin/env python3
"""Generates the content-pipeline sample files (needs Pillow):

  Content/Formats/sample.{png,jpg,bmp,tga,gif,tif,qoi}   one picture in seven loose-file formats
  Content/Formats/broken.png                              a PNG cut off in the middle (the failure exhibit)
  Content/Formats/notes.txt                               a text file for TitleContainer.OpenStream
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
