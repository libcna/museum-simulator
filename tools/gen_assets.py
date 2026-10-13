#!/usr/bin/env python3
"""Generates every asset in Content/ from scratch (pure Python 3 standard library).

All output is original and procedural, so the repository carries no third-party
asset licenses. Run from anywhere:

    python3 tools/gen_assets.py            # writes ../Content next to this script

Outputs are deterministic: running it twice produces identical files.

  Content/Textures/*.png   small tileable textures
  Content/Models/*.gltf    glTF 2.0 models (embedded base64 buffer)
  Content/Audio/*.wav      16-bit mono PCM sounds
"""
import base64
import json
import math
import os
import random
import struct
import sys
import wave
import zlib

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Content")


# ----------------------------------------------------------------------------
# PNG writing
# ----------------------------------------------------------------------------
def write_png(path, width, height, rgba):
    """rgba: bytes/bytearray of width*height*4."""
    raw = bytearray()
    stride = width * 4
    for y in range(height):
        raw.append(0)
        raw.extend(rgba[y * stride:(y + 1) * stride])

    def chunk(tag, data):
        body = tag + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(png)


def clamp8(v):
    return 0 if v < 0 else 255 if v > 255 else int(v)


class Image:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.px = [(0, 0, 0, 255)] * (w * h)

    def set(self, x, y, c):
        self.px[(y % self.h) * self.w + (x % self.w)] = c

    def get(self, x, y):
        return self.px[(y % self.h) * self.w + (x % self.w)]

    def save(self, name):
        buf = bytearray()
        for r, g, b, a in self.px:
            buf += bytes((clamp8(r), clamp8(g), clamp8(b), clamp8(a)))
        write_png(os.path.join(ROOT, "Textures", name), self.w, self.h, buf)


def lerp(a, b, t):
    return a + (b - a) * t


def mix(c0, c1, t):
    return tuple(lerp(c0[i], c1[i], t) for i in range(len(c0)))


# Tileable value noise ---------------------------------------------------------
def make_noise(size, cells, seed):
    rnd = random.Random(seed)
    lat = [[rnd.random() for _ in range(cells)] for _ in range(cells)]

    def smooth(t):
        return t * t * (3 - 2 * t)

    def sample(u, v):
        x, y = u * cells, v * cells
        x0, y0 = int(math.floor(x)) % cells, int(math.floor(y)) % cells
        x1, y1 = (x0 + 1) % cells, (y0 + 1) % cells
        fx, fy = smooth(x - math.floor(x)), smooth(y - math.floor(y))
        a = lerp(lat[y0][x0], lat[y0][x1], fx)
        b = lerp(lat[y1][x0], lat[y1][x1], fx)
        return lerp(a, b, fy)

    return sample


def fbm(size, seed, base=4, octaves=4):
    samplers = [make_noise(size, base * (2 ** o), seed + o) for o in range(octaves)]
    out = []
    for y in range(size):
        for x in range(size):
            u, v = x / size, y / size
            total, amp, norm = 0.0, 1.0, 0.0
            for o, s in enumerate(samplers):
                total += s(u, v) * amp
                norm += amp
                amp *= 0.5
            out.append(total / norm)
    return out


# ----------------------------------------------------------------------------
# Textures
# ----------------------------------------------------------------------------
def tex_checker():
    img = Image(128, 128)
    for y in range(128):
        for x in range(128):
            on = ((x // 16) + (y // 16)) % 2 == 0
            img.set(x, y, (235, 235, 235, 255) if on else (40, 40, 48, 255))
    img.save("checker.png")


def tex_uvgrid():
    """Colourful grid whose cells encode U and V, so UV mapping is visible."""
    n = 256
    img = Image(n, n)
    for y in range(n):
        for x in range(n):
            cu, cv = x // 32, y // 32
            base = (int(255 * x / n), int(255 * y / n), 160)
            if (cu + cv) % 2:
                base = tuple(int(c * 0.78) for c in base)
            if x % 32 == 0 or y % 32 == 0:
                base = (20, 20, 20)
            img.set(x, y, base + (255,))
    # Arrow in the first cell marks the UV origin (top-left).
    for i in range(24):
        for j in range(24):
            if i <= j + 2 and (i < 4 or j < 4):
                img.set(4 + i, 4 + j, (255, 255, 255, 255))
    img.save("uvgrid.png")


def tex_brick():
    n = 128
    noise = fbm(n, 11, 8, 3)
    img = Image(n, n)
    bh, bw = 16, 32
    for y in range(n):
        row = y // bh
        off = (bw // 2) if row % 2 else 0
        for x in range(n):
            bx = (x + off) % bw
            mortar = (y % bh) < 2 or bx < 2
            nz = noise[y * n + x]
            if mortar:
                c = (170 + nz * 25,) * 3
            else:
                shade = 0.8 + 0.4 * ((hash((row, (x + off) // bw)) & 0xFF) / 255.0)
                c = (150 * shade + nz * 40, 70 * shade + nz * 25, 52 * shade + nz * 20)
            img.set(x, y, tuple(clamp8(v) for v in c) + (255,))
    img.save("brick.png")


def tex_stone():
    n = 128
    noise = fbm(n, 21, 6, 5)
    img = Image(n, n)
    for y in range(n):
        for x in range(n):
            v = noise[y * n + x]
            g = 120 + v * 90
            img.set(x, y, (clamp8(g), clamp8(g * 0.98), clamp8(g * 0.94), 255))
    img.save("stone.png")


def tex_marble():
    n = 128
    noise = fbm(n, 31, 4, 5)
    img = Image(n, n)
    for y in range(n):
        for x in range(n):
            v = noise[y * n + x]
            vein = abs(math.sin((x / n * 3.0 + v * 6.0) * math.pi))
            vein = vein ** 6
            g = 232 - 70 * vein - 18 * v
            img.set(x, y, (clamp8(g), clamp8(g), clamp8(g + 6), 255))
    img.save("marble.png")


def tex_wood():
    n = 128
    noise = fbm(n, 41, 4, 4)
    img = Image(n, n)
    for y in range(n):
        for x in range(n):
            v = noise[y * n + x]
            ring = math.sin((y / n * 14.0 + v * 4.0) * math.pi * 2.0) * 0.5 + 0.5
            c = mix((120, 78, 40), (176, 122, 70), ring * 0.8 + v * 0.2)
            img.set(x, y, tuple(clamp8(k) for k in c) + (255,))
    img.save("wood.png")


def tex_tiles():
    n = 128
    noise = fbm(n, 51, 6, 3)
    img = Image(n, n)
    for y in range(n):
        for x in range(n):
            grout = (x % 32) < 2 or (y % 32) < 2
            checker = ((x // 32) + (y // 32)) % 2
            v = noise[y * n + x]
            if grout:
                c = (90, 90, 96)
            else:
                c = (200, 205, 210) if checker else (60, 70, 90)
                c = tuple(k * (0.93 + 0.12 * v) for k in c)
            img.set(x, y, tuple(clamp8(k) for k in c) + (255,))
    img.save("tiles.png")


def tex_carpet():
    n = 64
    rnd = random.Random(61)
    img = Image(n, n)
    for y in range(n):
        for x in range(n):
            k = rnd.random()
            weave = 0.88 + 0.12 * (((x // 2) + (y // 2)) % 2)
            c = (110 * weave + k * 22, 28 * weave + k * 8, 40 * weave + k * 10)
            img.set(x, y, tuple(clamp8(v) for v in c) + (255,))
    img.save("carpet.png")


def tex_plaster():
    n = 128
    noise = fbm(n, 71, 8, 4)
    img = Image(n, n)
    for y in range(n):
        for x in range(n):
            v = noise[y * n + x]
            g = 226 + (v - 0.5) * 26
            img.set(x, y, (clamp8(g), clamp8(g - 3), clamp8(g - 10), 255))
    img.save("plaster.png")


def tex_metal():
    n = 128
    rnd = random.Random(81)
    img = Image(n, n)
    for y in range(n):
        for x in range(n):
            streak = 0.5 + 0.5 * math.sin(x * 0.9) * 0.15 + rnd.random() * 0.06
            g = 150 + streak * 70
            img.set(x, y, (clamp8(g), clamp8(g + 3), clamp8(g + 10), 255))
    img.save("metal.png")


def tex_grass():
    n = 128
    noise = fbm(n, 91, 12, 4)
    rnd = random.Random(92)
    img = Image(n, n)
    for y in range(n):
        for x in range(n):
            v = noise[y * n + x]
            blade = rnd.random() * 0.25
            c = mix((36, 82, 30), (96, 150, 52), v * 0.8 + blade)
            img.set(x, y, tuple(clamp8(k) for k in c) + (255,))
    img.save("grass.png")


def tex_cobble():
    n = 128
    noise = fbm(n, 101, 8, 3)
    img = Image(n, n)
    centres = []
    rnd = random.Random(102)
    for gy in range(4):
        for gx in range(4):
            centres.append(((gx + 0.5 + (rnd.random() - 0.5) * 0.5) * 32,
                            (gy + 0.5 + (rnd.random() - 0.5) * 0.5) * 32))
    for y in range(n):
        for x in range(n):
            d1 = d2 = 1e9
            for cx, cy in centres:
                for ox in (-n, 0, n):
                    for oy in (-n, 0, n):
                        d = math.hypot(x - cx - ox, y - cy - oy)
                        if d < d1:
                            d1, d2 = d, d1
                        elif d < d2:
                            d2 = d
            edge = d2 - d1
            v = noise[y * n + x]
            if edge < 2.2:
                c = (70, 66, 62)
            else:
                c = (128 + v * 60, 124 + v * 56, 116 + v * 50)
            img.set(x, y, tuple(clamp8(k) for k in c) + (255,))
    img.save("cobble.png")


def tex_clouds():
    n = 256
    noise = fbm(n, 111, 4, 6)
    img = Image(n, n)
    for y in range(n):
        for x in range(n):
            v = noise[y * n + x]
            a = max(0.0, min(1.0, (v - 0.48) * 4.2))
            img.set(x, y, (255, 255, 255, clamp8(a * 235)))
    img.save("clouds.png")


def tex_particle():
    n = 64
    img = Image(n, n)
    for y in range(n):
        for x in range(n):
            d = math.hypot((x + 0.5) / n - 0.5, (y + 0.5) / n - 0.5) * 2.0
            a = max(0.0, 1.0 - d)
            a = a * a * (3 - 2 * a)
            img.set(x, y, (255, 255, 255, clamp8(a * 255)))
    img.save("particle.png")


def tex_smoke():
    n = 64
    noise = fbm(n, 121, 4, 3)
    img = Image(n, n)
    for y in range(n):
        for x in range(n):
            d = math.hypot((x + 0.5) / n - 0.5, (y + 0.5) / n - 0.5) * 2.0
            a = max(0.0, 1.0 - d)
            a = a * a * (0.6 + 0.8 * noise[y * n + x])
            img.set(x, y, (255, 255, 255, clamp8(min(1.0, a) * 255)))
    img.save("smoke.png")


def tex_glass():
    n = 64
    img = Image(n, n)
    for y in range(n):
        for x in range(n):
            edge = min(x, y, n - 1 - x, n - 1 - y)
            a = 70 if edge > 2 else 190
            diag = abs(((x + y) % 40) - 20) < 2
            if diag and edge > 2:
                a = 120
            img.set(x, y, (210, 235, 255, a))
    img.save("glass.png")


def tex_concrete():
    n = 128
    noise = fbm(n, 301, 6, 5)
    img = Image(n, n)
    for y in range(n):
        for x in range(n):
            g = 168 + (noise[y * n + x] - 0.5) * 46
            if x % 64 == 0 or y % 64 == 0:      # a slab joint every half tile
                g -= 38
            img.set(x, y, (clamp8(g), clamp8(g), clamp8(g + 3), 255))
    img.save("concrete.png")


def tex_panel():
    """White wall panels with thin seams: a quiet laboratory wall."""
    n = 128
    noise = fbm(n, 311, 5, 3)
    img = Image(n, n)
    for y in range(n):
        for x in range(n):
            g = 236 + (noise[y * n + x] - 0.5) * 10
            if x % 64 < 2 or y % 64 < 2:
                g -= 40
            img.set(x, y, (clamp8(g), clamp8(g + 1), clamp8(g + 4), 255))
    img.save("panel.png")


def tex_darkgrid():
    n = 128
    img = Image(n, n)
    for y in range(n):
        for x in range(n):
            g = 26 + ((x // 64) + (y // 64)) % 2 * 4
            if x % 32 < 2 or y % 32 < 2:
                g = 62
            img.set(x, y, (clamp8(g), clamp8(g + 4), clamp8(g + 14), 255))
    img.save("darkgrid.png")


def tex_hexagons():
    n = 128
    img = Image(n, n)
    s = 21.0
    for y in range(n):
        for x in range(n):
            # distance to the nearest hexagon centre in a flat-top lattice
            q = (x / s) * (2.0 / 3.0)
            r = (-x / s) / 3.0 + (math.sqrt(3.0) / 3.0) * (y / s)
            cx, cz = q, r
            cy = -cx - cz
            rx, ry, rz = round(cx), round(cy), round(cz)
            dx, dy, dz = abs(rx - cx), abs(ry - cy), abs(rz - cz)
            edge = 1.0 - max(dx, dy, dz) * 1.8
            g = 70 + 60 * max(0.0, min(1.0, edge * 3))
            if edge < 0.12:
                g = 200
            img.set(x, y, (clamp8(g * 0.7), clamp8(g * 0.9), clamp8(g + 20), 255))
    img.save("hexagons.png")


def tex_circuit():
    n = 128
    rnd = random.Random(331)
    img = Image(n, n)
    for y in range(n):
        for x in range(n):
            img.set(x, y, (14, 40, 34, 255))
    for _ in range(60):
        x, y = rnd.randrange(8, n - 8), rnd.randrange(8, n - 8)
        for _ in range(rnd.randrange(3, 9)):
            horizontal = rnd.random() < 0.5
            length = rnd.randrange(6, 26)
            for i in range(length):
                px, py = (x + i, y) if horizontal else (x, y + i)
                img.set(px % n, py % n, (60, 210, 150, 255))
            x, y = (x + length, y) if horizontal else (x, y + length)
        img.set(x % n, y % n, (230, 220, 120, 255))
    img.save("circuit.png")


def tex_sand():
    n = 128
    noise = fbm(n, 341, 8, 5)
    rnd = random.Random(342)
    img = Image(n, n)
    for y in range(n):
        for x in range(n):
            g = 200 + (noise[y * n + x] - 0.5) * 40 + rnd.random() * 10
            img.set(x, y, (clamp8(g), clamp8(g * 0.9), clamp8(g * 0.7), 255))
    img.save("sand.png")


def tex_hedge():
    n = 128
    noise = fbm(n, 351, 10, 4)
    rnd = random.Random(352)
    img = Image(n, n)
    for y in range(n):
        for x in range(n):
            g = 60 + noise[y * n + x] * 80 + rnd.random() * 30
            img.set(x, y, (clamp8(g * 0.45), clamp8(g), clamp8(g * 0.4), 255))
    img.save("hedge.png")


def tex_rust():
    n = 128
    noise = fbm(n, 361, 6, 5)
    img = Image(n, n)
    for y in range(n):
        for x in range(n):
            v = noise[y * n + x]
            img.set(x, y, (clamp8(110 + v * 100), clamp8(55 + v * 50), clamp8(30 + v * 25), 255))
    img.save("rust.png")


def tex_gradient():
    """A horizontal hue ramp with a vertical fade: shows interpolation, filtering and clamping clearly."""
    n = 128
    img = Image(n, n)
    for y in range(n):
        for x in range(n):
            h = x / n * 6.0
            c = [max(0.0, min(1.0, abs(h - 3) - 1)), max(0.0, min(1.0, 2 - abs(h - 2))), max(0.0, min(1.0, 2 - abs(h - 4)))]
            f = 1.0 - 0.6 * (y / n)
            img.set(x, y, (clamp8(c[0] * 255 * f), clamp8(c[1] * 255 * f), clamp8(c[2] * 255 * f), 255))
    img.save("gradient.png")


def tex_pixelart():
    """A 16x16 pixel-art sprite sheet (2x2 frames of 8x8): a coin spinning, for point-sampling demonstrations."""
    img = Image(16, 16)
    pal = {".": (0, 0, 0, 0), "o": (120, 80, 0, 255), "y": (255, 214, 60, 255), "h": (255, 245, 170, 255), "s": (200, 150, 20, 255)}
    frames = [
        ["..oooo..", ".oyyhyo.", "oyyhyyso", "oyhyyyso", "oyyyyyso", "oyyyysso", ".oysyso.", "..oooo.."],
        ["..oooo..", ".oyyhyo.", ".oyhyyo.", ".oyhyyo.", ".oyyyso.", ".oyyyso.", ".oysyso.", "..oooo.."],
        ["...oo...", "..oyho..", "..oyhso.", "..oyyso.", "..oyyso.", "..oyyso.", "..oyso..", "...oo..."],
        ["..oooo..", ".oyhyyo.", ".oyhyyo.", ".oyyyso.", ".oyyyso.", ".oyyyso.", ".oysyso.", "..oooo.."],
    ]
    for f, rows in enumerate(frames):
        ox, oy = (f % 2) * 8, (f // 2) * 8
        for y, row in enumerate(rows):
            for x, ch in enumerate(row):
                img.set(ox + x, oy + y, pal[ch])
    img.save("coin.png")


def tex_logo():
    """A stylised CNA mark: three coloured bars on a dark plate."""
    n = 256
    img = Image(n, n)
    for y in range(n):
        for x in range(n):
            img.set(x, y, (18, 22, 34, 255))
    cols = [(236, 96, 80), (96, 190, 120), (90, 150, 240)]
    for i, col in enumerate(cols):
        x0 = 36 + i * 64
        for y in range(56, 200):
            for x in range(x0, x0 + 44):
                shade = 1.0 - (y - 56) / 300.0
                img.set(x, y, tuple(clamp8(k * shade) for k in col) + (255,))
    img.save("logo.png")


# ----------------------------------------------------------------------------
# glTF models
# ----------------------------------------------------------------------------
class Mesh:
    def __init__(self):
        self.pos, self.nrm, self.uv, self.idx = [], [], [], []

    def vert(self, p, n, uv):
        self.pos.append(p)
        self.nrm.append(n)
        self.uv.append(uv)
        return len(self.pos) - 1

    def tri(self, a, b, c):
        self.idx += [a, b, c]


def norm(v):
    l = math.sqrt(sum(c * c for c in v)) or 1.0
    return tuple(c / l for c in v)


def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def grid_mesh(fn, nu, nv, wrap_u=False):
    """Parametric surface. fn(u, v) -> (position, normal). Triangles are counter-clockwise
    seen from outside (glTF front faces)."""
    m = Mesh()
    for j in range(nv + 1):
        for i in range(nu + 1):
            u, v = i / nu, j / nv
            p, n = fn(u, v)
            m.vert(p, n, (u, v))
    row = nu + 1
    for j in range(nv):
        for i in range(nu):
            a = j * row + i
            b = a + 1
            c = a + row
            d = c + 1
            m.tri(a, b, c)
            m.tri(b, d, c)
    return m


def sphere_mesh(r, nu=32, nv=16):
    def fn(u, v):
        th, ph = u * 2 * math.pi, v * math.pi
        n = (math.sin(ph) * math.cos(th), math.cos(ph), math.sin(ph) * math.sin(th))
        return tuple(c * r for c in n), n
    # u around, v from top to bottom: orientation chosen so winding faces outward.
    m = grid_mesh(fn, nu, nv)
    flip_winding(m)
    return m


def torus_mesh(R, r, nu=40, nv=20):
    def fn(u, v):
        th, ph = u * 2 * math.pi, v * 2 * math.pi
        cx, cz = math.cos(th), math.sin(th)
        n = (math.cos(ph) * cx, math.sin(ph), math.cos(ph) * cz)
        p = ((R + r * math.cos(ph)) * cx, r * math.sin(ph), (R + r * math.cos(ph)) * cz)
        return p, n
    m = grid_mesh(fn, nu, nv)
    flip_winding(m)
    return m


def flip_winding(m):
    for i in range(0, len(m.idx), 3):
        m.idx[i + 1], m.idx[i + 2] = m.idx[i + 2], m.idx[i + 1]


def check_winding(m):
    """Self-check: make triangles wind counter-clockwise relative to their stored normals."""
    bad = 0
    for i in range(0, len(m.idx), 3):
        a, b, c = m.idx[i], m.idx[i + 1], m.idx[i + 2]
        fn = cross(sub(m.pos[b], m.pos[a]), sub(m.pos[c], m.pos[a]))
        n = tuple(m.nrm[a][k] + m.nrm[b][k] + m.nrm[c][k] for k in range(3))
        if sum(fn[k] * n[k] for k in range(3)) < -1e-9:
            bad += 1
    return bad


def orient_triangles(m):
    """Flip every triangle whose geometric normal disagrees with its stored vertex normals, so
    all faces are counter-clockwise seen from outside (glTF front faces)."""
    for i in range(0, len(m.idx), 3):
        a, b, c = m.idx[i], m.idx[i + 1], m.idx[i + 2]
        fn = cross(sub(m.pos[b], m.pos[a]), sub(m.pos[c], m.pos[a]))
        n = tuple(m.nrm[a][k] + m.nrm[b][k] + m.nrm[c][k] for k in range(3))
        if sum(fn[k] * n[k] for k in range(3)) < 0:
            m.idx[i + 1], m.idx[i + 2] = m.idx[i + 2], m.idx[i + 1]


def lathe_mesh(profile, segs=32):
    """profile: list of (radius, height) from bottom to top; surface of revolution."""
    nv = len(profile) - 1

    def fn(u, v):
        t = v * nv
        k = min(int(t), nv - 1)
        f = t - k
        r = lerp(profile[k][0], profile[k + 1][0], f)
        h = lerp(profile[k][1], profile[k + 1][1], f)
        dr = profile[k + 1][0] - profile[k][0]
        dh = profile[k + 1][1] - profile[k][1]
        th = u * 2 * math.pi
        cx, cz = math.cos(th), math.sin(th)
        n = norm((dh * cx, -dr, dh * cz))
        return (r * cx, h, r * cz), n
    m = grid_mesh(fn, segs, nv)
    # Ensure outward winding by checking one triangle against its normal.
    if check_winding(m) > len(m.idx) // 6:
        flip_winding(m)
    return m


def box_mesh(sx, sy, sz):
    m = Mesh()
    faces = [
        ((0, 0, 1), (1, 0, 0), (0, 1, 0)),
        ((0, 0, -1), (-1, 0, 0), (0, 1, 0)),
        ((1, 0, 0), (0, 0, -1), (0, 1, 0)),
        ((-1, 0, 0), (0, 0, 1), (0, 1, 0)),
        ((0, 1, 0), (1, 0, 0), (0, 0, -1)),
        ((0, -1, 0), (1, 0, 0), (0, 0, 1)),
    ]
    half = (sx / 2, sy / 2, sz / 2)
    for n, right, up in faces:
        c = tuple(n[k] * half[k] for k in range(3))
        pts = []
        for (a, b) in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            pts.append(tuple(c[k] + right[k] * a * half[k] + up[k] * b * half[k] for k in range(3)))
        base = m.vert(pts[0], n, (0, 1))
        m.vert(pts[1], n, (1, 1))
        m.vert(pts[2], n, (1, 0))
        m.vert(pts[3], n, (0, 0))
        m.tri(base, base + 1, base + 2)
        m.tri(base, base + 2, base + 3)
    return m


def cone_mesh(r, h, segs=32):
    return lathe_mesh([(r, 0.0), (r * 0.999, 0.0001), (0.0001, h)], segs)


def star_mesh(points=5, outer=0.55, inner=0.24, depth=0.18):
    """Extruded star: front/back caps plus side walls, flat shaded."""
    m = Mesh()
    ring = []
    for i in range(points * 2):
        a = math.pi / 2 + i * math.pi / points
        r = outer if i % 2 == 0 else inner
        ring.append((math.cos(a) * r, math.sin(a) * r))
    zf, zb = depth / 2, -depth / 2
    # caps (fan around centre)
    for z, nz in ((zf, 1.0), (zb, -1.0)):
        c = m.vert((0, 0, z), (0, 0, nz), (0.5, 0.5))
        vs = [m.vert((x, y, z), (0, 0, nz), (0.5 + x / (2 * outer), 0.5 - y / (2 * outer))) for x, y in ring]
        for i in range(len(vs)):
            a, b = vs[i], vs[(i + 1) % len(vs)]
            if nz > 0:
                m.tri(c, a, b)
            else:
                m.tri(c, b, a)
    for i in range(len(ring)):
        x0, y0 = ring[i]
        x1, y1 = ring[(i + 1) % len(ring)]
        nx, ny = y1 - y0, -(x1 - x0)
        n = norm((nx, ny, 0))
        a = m.vert((x0, y0, zf), n, (0, 0))
        b = m.vert((x1, y1, zf), n, (1, 0))
        c = m.vert((x1, y1, zb), n, (1, 1))
        d = m.vert((x0, y0, zb), n, (0, 1))
        m.tri(a, b, c)
        m.tri(a, c, d)
    # side walls were generated with a clockwise ring; verify and fix.
    if check_winding(m) > len(m.idx) // 6:
        flip_winding(m)
    return m


def knot_mesh(nu=96, nv=12, tube=0.09, scale=0.22):
    """Trefoil knot tube."""
    def curve(t):
        x = math.sin(t) + 2 * math.sin(2 * t)
        y = math.cos(t) - 2 * math.cos(2 * t)
        z = -math.sin(3 * t)
        return (x * scale, z * scale, y * scale)

    def fn(u, v):
        t = u * 2 * math.pi
        p = curve(t)
        p2 = curve(t + 0.001)
        tang = norm(sub(p2, p))
        ref = (0, 1, 0) if abs(tang[1]) < 0.9 else (1, 0, 0)
        nrm1 = norm(cross(tang, ref))
        nrm2 = cross(tang, nrm1)
        ph = v * 2 * math.pi
        n = tuple(math.cos(ph) * nrm1[k] + math.sin(ph) * nrm2[k] for k in range(3))
        return tuple(p[k] + n[k] * tube for k in range(3)), n
    m = grid_mesh(fn, nu, nv)
    if check_winding(m) > len(m.idx) // 6:
        flip_winding(m)
    return m


def write_gltf(name, meshes):
    """meshes: list of (Mesh, material dict). Each becomes one node/mesh primitive."""
    blob = bytearray()
    views, accessors, materials, gmeshes, nodes = [], [], [], [], []

    def add_view(data, target):
        while len(blob) % 4:
            blob.append(0)
        off = len(blob)
        blob.extend(data)
        views.append({"buffer": 0, "byteOffset": off, "byteLength": len(data), "target": target})
        return len(views) - 1

    for mesh, mat in meshes:
        orient_triangles(mesh)
        pos = b"".join(struct.pack("<3f", *p) for p in mesh.pos)
        nrm = b"".join(struct.pack("<3f", *n) for n in mesh.nrm)
        uv = b"".join(struct.pack("<2f", *t) for t in mesh.uv)
        idx = b"".join(struct.pack("<H", i) for i in mesh.idx)
        if len(mesh.pos) > 65535:
            raise SystemExit("mesh too large for 16-bit indices: " + name)
        mn = [min(p[k] for p in mesh.pos) for k in range(3)]
        mx = [max(p[k] for p in mesh.pos) for k in range(3)]
        vp = add_view(pos, 34962)
        vn = add_view(nrm, 34962)
        vt = add_view(uv, 34962)
        vi = add_view(idx, 34963)
        base = len(accessors)
        accessors.append({"bufferView": vp, "componentType": 5126, "count": len(mesh.pos),
                          "type": "VEC3", "min": mn, "max": mx})
        accessors.append({"bufferView": vn, "componentType": 5126, "count": len(mesh.nrm), "type": "VEC3"})
        accessors.append({"bufferView": vt, "componentType": 5126, "count": len(mesh.uv), "type": "VEC2"})
        accessors.append({"bufferView": vi, "componentType": 5123, "count": len(mesh.idx), "type": "SCALAR"})
        materials.append({"name": mat.get("name", "mat"), "pbrMetallicRoughness": {
            "baseColorFactor": list(mat["color"]) + [1.0],
            "metallicFactor": mat.get("metal", 0.0), "roughnessFactor": mat.get("rough", 0.6)}})
        gmeshes.append({"primitives": [{"attributes": {"POSITION": base, "NORMAL": base + 1,
                                                       "TEXCOORD_0": base + 2},
                                        "indices": base + 3, "material": len(materials) - 1}]})
        nodes.append({"mesh": len(gmeshes) - 1, "name": mat.get("name", "mesh")})
    doc = {
        "asset": {"version": "2.0", "generator": "cna-3d-museum tools/gen_assets.py"},
        "scene": 0, "scenes": [{"nodes": list(range(len(nodes)))}], "nodes": nodes,
        "meshes": gmeshes, "materials": materials, "accessors": accessors,
        "bufferViews": views,
        "buffers": [{"byteLength": len(blob),
                     "uri": "data:application/octet-stream;base64," + base64.b64encode(bytes(blob)).decode()}],
    }
    path = os.path.join(ROOT, "Models", name + ".gltf")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as f:
        json.dump(doc, f, separators=(",", ":"))


def build_models():
    write_gltf("cube", [(box_mesh(1, 1, 1), {"name": "cube", "color": (0.85, 0.35, 0.30)})])
    write_gltf("sphere", [(sphere_mesh(0.5), {"name": "sphere", "color": (0.30, 0.55, 0.90)})])
    write_gltf("torus", [(torus_mesh(0.36, 0.14), {"name": "torus", "color": (0.95, 0.75, 0.25)})])
    write_gltf("cone", [(cone_mesh(0.42, 0.9), {"name": "cone", "color": (0.35, 0.80, 0.50)})])
    write_gltf("star", [(star_mesh(), {"name": "star", "color": (0.95, 0.88, 0.30)})])
    write_gltf("knot", [(knot_mesh(), {"name": "knot", "color": (0.75, 0.40, 0.85)})])
    vase = [(0.0, 0.0), (0.22, 0.0), (0.26, 0.04), (0.34, 0.18), (0.40, 0.36), (0.38, 0.52),
            (0.26, 0.68), (0.18, 0.80), (0.20, 0.90), (0.26, 0.98), (0.0, 0.98)]
    # The lathe profile starts and ends on the axis; those two rim edges are the bottom and
    # top cap (flat shaded by the profile normal), which is fine for a display vase.
    write_gltf("vase", [(lathe_mesh(vase, 40), {"name": "vase", "color": (0.80, 0.52, 0.34), "rough": 0.4})])


# ----------------------------------------------------------------------------
# Animated glTF models: a rigid robot arm (node hierarchy + animation) and a skinned tentacle
# ----------------------------------------------------------------------------
class GltfDoc:
    """A tiny glTF 2.0 writer for hand-built buffers (positions, skinning, animation)."""

    def __init__(self):
        self.blob = bytearray()
        self.views, self.accessors = [], []

    def view(self, data, target=None):
        while len(self.blob) % 4:
            self.blob.append(0)
        v = {"buffer": 0, "byteOffset": len(self.blob), "byteLength": len(data)}
        if target:
            v["target"] = target
        self.blob.extend(data)
        self.views.append(v)
        return len(self.views) - 1

    def accessor(self, data, comp, count, typ, target=None, minmax=None):
        a = {"bufferView": self.view(data, target), "componentType": comp, "count": count, "type": typ}
        if minmax:
            a["min"], a["max"] = minmax
        self.accessors.append(a)
        return len(self.accessors) - 1

    def finish(self, name, extra):
        doc = {"asset": {"version": "2.0", "generator": "cna-3d-museum tools/gen_assets.py"}}
        doc.update(extra)
        doc["accessors"], doc["bufferViews"] = self.accessors, self.views
        doc["buffers"] = [{"byteLength": len(self.blob), "uri": "data:application/octet-stream;base64," + base64.b64encode(bytes(self.blob)).decode()}]
        path = os.path.join(ROOT, "Models", name + ".gltf")
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w") as f:
            json.dump(doc, f, separators=(",", ":"))


def quat_z(angle):
    return (0.0, 0.0, math.sin(angle / 2), math.cos(angle / 2))


def quat_y(angle):
    return (0.0, math.sin(angle / 2), 0.0, math.cos(angle / 2))


def build_robot():
    g = GltfDoc()
    meshes, materials, nodes = [], [], []

    def add_box(w, h, d, colour, rough=0.45, metal=0.2):
        m = box_mesh(w, h, d)
        for i, (x, y, z) in enumerate(m.pos):
            m.pos[i] = (x, y + h / 2, z)       # box stands on the node origin: joints sit at the bottom
        orient_triangles(m)
        pos = g.accessor(b"".join(struct.pack("<3f", *p) for p in m.pos), 5126, len(m.pos), "VEC3", 34962,
                         ([min(p[k] for p in m.pos) for k in range(3)], [max(p[k] for p in m.pos) for k in range(3)]))
        nrm = g.accessor(b"".join(struct.pack("<3f", *n) for n in m.nrm), 5126, len(m.nrm), "VEC3", 34962)
        uv = g.accessor(b"".join(struct.pack("<2f", *t) for t in m.uv), 5126, len(m.uv), "VEC2", 34962)
        idx = g.accessor(b"".join(struct.pack("<H", i) for i in m.idx), 5123, len(m.idx), "SCALAR", 34963)
        materials.append({"pbrMetallicRoughness": {"baseColorFactor": list(colour) + [1.0], "metallicFactor": metal, "roughnessFactor": rough}})
        meshes.append({"primitives": [{"attributes": {"POSITION": pos, "NORMAL": nrm, "TEXCOORD_0": uv}, "indices": idx, "material": len(materials) - 1}]})
        return len(meshes) - 1

    base = add_box(0.9, 0.25, 0.9, (0.35, 0.37, 0.42))
    upper = add_box(0.22, 1.0, 0.22, (0.90, 0.55, 0.15))
    lower = add_box(0.18, 0.85, 0.18, (0.85, 0.40, 0.12))
    hand = add_box(0.30, 0.18, 0.12, (0.30, 0.65, 0.85))
    finger = add_box(0.05, 0.22, 0.05, (0.30, 0.65, 0.85))
    nodes = [
        {"name": "base", "mesh": base, "children": [1]},
        {"name": "shoulder", "translation": [0, 0.25, 0], "children": [2]},
        {"name": "upperArm", "mesh": upper, "children": [3]},
        {"name": "elbow", "translation": [0, 1.0, 0], "children": [4]},
        {"name": "lowerArm", "mesh": lower, "children": [5]},
        {"name": "wrist", "translation": [0, 0.85, 0], "children": [6, 7, 8]},
        {"name": "hand", "mesh": hand},
        {"name": "fingerL", "mesh": finger, "translation": [-0.1, 0.18, 0]},
        {"name": "fingerR", "mesh": finger, "translation": [0.1, 0.18, 0]},
    ]
    times = [i * 0.5 for i in range(9)]   # 4 s
    def track(target, fn):
        t = g.accessor(b"".join(struct.pack("<f", x) for x in times), 5126, len(times), "SCALAR", None, ([times[0]], [times[-1]]))
        r = g.accessor(b"".join(struct.pack("<4f", *fn(x)) for x in times), 5126, len(times), "VEC4")
        return t, r, target
    chans = [track(1, lambda x: quat_y(math.sin(x / 4 * math.tau) * 1.2)), track(3, lambda x: quat_z(0.9 + 0.8 * math.sin(x / 4 * math.tau + 1.0))),
             track(5, lambda x: quat_z(0.5 * math.sin(x / 4 * math.tau * 2)))]
    samplers = [{"input": t, "output": r, "interpolation": "LINEAR"} for t, r, _ in chans]
    channels = [{"sampler": i, "target": {"node": c[2], "path": "rotation"}} for i, c in enumerate(chans)]
    g.finish("robot", {"scene": 0, "scenes": [{"nodes": [0]}], "nodes": nodes, "meshes": meshes, "materials": materials,
                       "animations": [{"name": "wave", "samplers": samplers, "channels": channels}]})


def build_tentacle():
    g = GltfDoc()
    segs, ring, seg_h, radius = 6, 12, 0.5, 0.22
    pos, nrm, uvs, jnt, wgt, idx = [], [], [], [], [], []
    rows = segs * 3 + 1
    for r in range(rows):
        y = r / (rows - 1) * segs * seg_h
        t = y / seg_h                      # position along the chain in joint units
        j = min(int(t), segs - 1)
        f = t - j
        taper = radius * (1.0 - 0.7 * y / (segs * seg_h))
        for k in range(ring + 1):
            a = k / ring * math.tau
            pos.append((math.cos(a) * taper, y, math.sin(a) * taper))
            nrm.append((math.cos(a), 0.0, math.sin(a)))
            uvs.append((k / ring, y / (segs * seg_h)))
            # Blend between this joint and the next: smooth bending between the joints.
            j1 = min(j + 1, segs - 1)
            jnt.append((j, j1, 0, 0))
            wgt.append((1.0 - f, f, 0.0, 0.0) if j1 != j else (1.0, 0.0, 0.0, 0.0))
    for r in range(rows - 1):
        for k in range(ring):
            a = r * (ring + 1) + k
            b = a + 1
            c = a + ring + 1
            d = c + 1
            idx += [a, c, b, b, c, d]
    m_pos = g.accessor(b"".join(struct.pack("<3f", *p) for p in pos), 5126, len(pos), "VEC3", 34962,
                       ([min(p[k] for p in pos) for k in range(3)], [max(p[k] for p in pos) for k in range(3)]))
    m_nrm = g.accessor(b"".join(struct.pack("<3f", *n) for n in nrm), 5126, len(nrm), "VEC3", 34962)
    m_uv = g.accessor(b"".join(struct.pack("<2f", *t) for t in uvs), 5126, len(uvs), "VEC2", 34962)
    m_jnt = g.accessor(b"".join(struct.pack("<4B", *j) for j in jnt), 5121, len(jnt), "VEC4", 34962)
    m_wgt = g.accessor(b"".join(struct.pack("<4f", *w) for w in wgt), 5126, len(wgt), "VEC4", 34962)
    m_idx = g.accessor(b"".join(struct.pack("<H", i) for i in idx), 5123, len(idx), "SCALAR", 34963)
    ibm = b"".join(struct.pack("<16f", 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, -i * seg_h, 0, 1) for i in range(segs))
    m_ibm = g.accessor(ibm, 5126, segs, "MAT4")
    # Nodes: 0 = mesh node, 1..segs = joint chain.
    nodes = [{"name": "tentacleMesh", "mesh": 0, "skin": 0}]
    for i in range(segs):
        n = {"name": "joint%d" % i}
        if i > 0:
            n["translation"] = [0, seg_h, 0]
        if i + 1 < segs:
            n["children"] = [i + 2]
        nodes.append(n)
    times = [i * 0.25 for i in range(17)]   # 4 s loop
    samplers, channels = [], []
    for j in range(segs):
        t = g.accessor(b"".join(struct.pack("<f", x) for x in times), 5126, len(times), "SCALAR", None, ([times[0]], [times[-1]]))
        r = g.accessor(b"".join(struct.pack("<4f", *quat_z(0.55 * math.sin(x / 4 * math.tau - j * 0.7))) for x in times), 5126, len(times), "VEC4")
        samplers.append({"input": t, "output": r, "interpolation": "LINEAR"})
        channels.append({"sampler": j, "target": {"node": j + 1, "path": "rotation"}})
    g.finish("tentacle", {"scene": 0, "scenes": [{"nodes": [0, 1]}], "nodes": nodes,
                          "meshes": [{"primitives": [{"attributes": {"POSITION": m_pos, "NORMAL": m_nrm, "TEXCOORD_0": m_uv, "JOINTS_0": m_jnt, "WEIGHTS_0": m_wgt}, "indices": m_idx, "material": 0}]}],
                          "materials": [{"pbrMetallicRoughness": {"baseColorFactor": [0.35, 0.80, 0.55, 1.0], "metallicFactor": 0.0, "roughnessFactor": 0.5}}],
                          "skins": [{"joints": list(range(1, segs + 1)), "inverseBindMatrices": m_ibm, "skeleton": 1}],
                          "animations": [{"name": "wave", "samplers": samplers, "channels": channels}]})


# ----------------------------------------------------------------------------
# Audio
# ----------------------------------------------------------------------------
RATE = 22050


def write_wav(name, samples):
    path = os.path.join(ROOT, "Audio", name + ".wav")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(b"".join(struct.pack("<h", int(max(-1.0, min(1.0, s)) * 32000)) for s in samples))


def loop_fade(samples, fade):
    """Cross-fade the tail into the head so the loop has no click."""
    n = len(samples)
    out = list(samples)
    for i in range(fade):
        t = i / fade
        out[i] = samples[i] * t + samples[n - fade + i] * (1 - t)
    return out[:n - fade]


def audio_ambience():
    rnd = random.Random(1)
    n = RATE * 6
    lp = 0.0
    s = []
    for i in range(n + RATE):
        lp += (rnd.uniform(-1, 1) - lp) * 0.02
        t = i / RATE
        hum = 0.10 * math.sin(2 * math.pi * 55 * t) + 0.05 * math.sin(2 * math.pi * 110.5 * t)
        s.append(lp * 0.9 + hum)
    write_wav("ambience", loop_fade(s, RATE // 2))


def audio_tone():
    n = RATE * 2
    s = []
    for i in range(n):
        t = i / RATE
        env = 0.5 + 0.5 * math.sin(2 * math.pi * 0.5 * t)
        s.append(0.35 * math.sin(2 * math.pi * 220 * t) * (0.6 + 0.4 * env)
                 + 0.15 * math.sin(2 * math.pi * 330 * t))
    write_wav("tone", s)


def audio_drums():
    rnd = random.Random(2)
    bpm_samples = RATE // 2
    n = bpm_samples * 8
    s = [0.0] * n
    for beat in range(8):
        start = beat * bpm_samples
        kick = beat % 2 == 0
        for i in range(int(RATE * (0.25 if kick else 0.12))):
            if start + i >= n:
                break
            t = i / RATE
            if kick:
                f = 120 * math.exp(-t * 18) + 40
                s[start + i] += 0.8 * math.sin(2 * math.pi * f * t) * math.exp(-t * 9)
            else:
                s[start + i] += 0.35 * rnd.uniform(-1, 1) * math.exp(-t * 30)
    write_wav("drums", s)


def audio_arpeggio():
    notes = [261.63, 329.63, 392.0, 523.25, 392.0, 329.63]
    step = RATE // 5
    n = step * len(notes)
    s = [0.0] * n
    for k, f in enumerate(notes):
        for i in range(step):
            t = i / RATE
            s[k * step + i] += 0.3 * math.sin(2 * math.pi * f * t) * math.exp(-t * 6)
            s[k * step + i] += 0.1 * math.sin(2 * math.pi * f * 2 * t) * math.exp(-t * 9)
    write_wav("arpeggio", s)


def audio_chirp():
    n = int(RATE * 0.6)
    s = []
    for i in range(n):
        t = i / RATE
        f = 500 + 1500 * t
        s.append(0.5 * math.sin(2 * math.pi * f * t) * math.exp(-t * 4))
    write_wav("chirp", s)


def audio_step():
    rnd = random.Random(3)
    n = int(RATE * 0.16)
    s = []
    lp = 0.0
    for i in range(n):
        t = i / RATE
        lp += (rnd.uniform(-1, 1) - lp) * 0.25
        s.append(0.7 * lp * math.exp(-t * 28) + 0.3 * math.sin(2 * math.pi * 70 * t) * math.exp(-t * 35))
    write_wav("step", s)


def audio_click():
    n = int(RATE * 0.08)
    s = [0.6 * math.sin(2 * math.pi * 1400 * (i / RATE)) * math.exp(-(i / RATE) * 70) for i in range(n)]
    write_wav("click", s)


def audio_splash():
    rnd = random.Random(4)
    n = RATE * 3
    lp = 0.0
    s = []
    for i in range(n + RATE // 2):
        lp += (rnd.uniform(-1, 1) - lp) * 0.12
        t = i / RATE
        mod = 0.65 + 0.35 * math.sin(2 * math.pi * 3.1 * t) * math.sin(2 * math.pi * 0.7 * t)
        s.append(lp * mod * 0.9)
    write_wav("water", loop_fade(s, RATE // 4))


def audio_fire():
    """Low rumble plus random crackles; loops."""
    rnd = random.Random(5)
    n = RATE * 4 + RATE // 2
    out = [0.0] * n
    lp = 0.0
    for i in range(n):
        lp += (rnd.uniform(-1, 1) - lp) * 0.03
        out[i] = lp * 0.55
    for _ in range(70):
        start = rnd.randrange(0, n - 800)
        amp = rnd.uniform(0.2, 0.9)
        length = rnd.randrange(80, 700)
        lp2 = 0.0
        for k in range(length):
            lp2 += (rnd.uniform(-1, 1) - lp2) * rnd.uniform(0.3, 0.9)
            out[start + k] += amp * lp2 * math.exp(-k / (length * 0.25))
    write_wav("fire", loop_fade(out, RATE // 2))


def audio_spark():
    rnd = random.Random(6)
    n = int(RATE * 0.5)
    s = []
    for i in range(n):
        t = i / RATE
        hiss = rnd.uniform(-1, 1) * math.exp(-t * 7) * 0.5
        ping = math.sin(2 * math.pi * (2200 - 1500 * t) * t) * math.exp(-t * 9) * 0.4
        s.append(hiss + ping)
    write_wav("spark", s)


def main():
    for fn in (tex_checker, tex_uvgrid, tex_brick, tex_stone, tex_marble, tex_wood, tex_tiles,
               tex_carpet, tex_plaster, tex_metal, tex_grass, tex_cobble, tex_clouds,
               tex_particle, tex_smoke, tex_glass, tex_logo, tex_concrete, tex_panel, tex_darkgrid, tex_hexagons,
               tex_circuit, tex_sand, tex_hedge, tex_rust, tex_gradient, tex_pixelart):
        fn()
    build_models()
    build_robot()
    build_tentacle()
    for fn in (audio_ambience, audio_tone, audio_drums, audio_arpeggio, audio_chirp, audio_step,
               audio_click, audio_splash, audio_fire, audio_spark):
        fn()
    print("wrote assets to", os.path.normpath(ROOT))


if __name__ == "__main__":
    main()
