#!/usr/bin/env python3
"""Writes Content/Custom/menu.xnb: an XNB file whose root object is the museum's own type, "Museum.Menu".

An XNB is XNA's compiled content container: a header, a table of the type readers needed, then the objects. A game
teaches ContentManager about its own type by writing a ContentTypeReader for it and registering the reader under the
name the file records. This script writes the file the *Content Readers* room loads; the reader is in the museum's
source (src/Exhibits/Content/ContentLab.cpp). Deterministic.

    python3 tools/gen_xnb.py
"""
import os
import struct

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Content", "Custom", "menu.xnb")
READER = "Museum.MenuReader"


def seven_bit(value):
    out = bytearray()
    while value >= 0x80:
        out.append((value & 0x7F) | 0x80)
        value >>= 7
    out.append(value)
    return bytes(out)


def string(text):
    raw = text.encode("utf-8")
    return seven_bit(len(raw)) + raw


def main():
    body = seven_bit(1) + string(READER) + struct.pack("<i", 0)      # one type reader, its version
    body += seven_bit(0)                                             # no shared resources
    body += seven_bit(1)                                             # the root object uses reader number 1
    body += string("Chef's special") + struct.pack("<i", 1250) + struct.pack("<f", 0.6)
    items = ["miso soup", "grilled aubergine", "rice", "green tea"]
    body += struct.pack("<i", len(items)) + b"".join(string(i) for i in items)
    header = b"XNBw" + bytes([5, 0]) + struct.pack("<i", 10 + len(body))   # platform w, version 5, uncompressed, file size
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "wb") as f:
        f.write(header + body)
    print("wrote", OUT, 10 + len(body), "bytes")


if __name__ == "__main__":
    main()
