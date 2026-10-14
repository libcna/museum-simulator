#!/usr/bin/env python3
"""Writes a small XACT project (settings, wave bank, sound bank) into Content/Xact from the museum's own WAV files.

XACT is XNA's authoring route for audio: a global settings file (.xgs: categories, variables and RPC curves), a wave bank (.xwb:
the PCM data) and a sound bank (.xsb: cues that play waves). Microsoft's authoring tool is not needed to *read* them,
and CNA's AudioEngine, WaveBank, SoundBank and Cue load the plain binary formats, so this script writes them directly.
The layouts follow what CNA's own audio tests build. Deterministic: running it twice gives identical files.

    python3 tools/gen_xact.py          # needs Content/Audio/*.wav (tools/gen_assets.py makes them)
"""
import os
import struct
import wave

HERE = os.path.dirname(os.path.abspath(__file__))
AUDIO = os.path.join(HERE, "..", "Content", "Audio")
OUT = os.path.join(HERE, "..", "Content", "Xact")

CATEGORIES = ["Default", "Music", "Effects"]
# cue name, source wave, category index, longest part kept (seconds)
CUES = [
    ("Tone", "tone", 0, 0.7),
    ("Click", "click", 0, 0.5),
    ("Arpeggio", "arpeggio", 1, 0.9),
    ("Drums", "drums", 1, 0.9),
    ("Chirp", "chirp", 2, 0.6),
]
WAVEBANK = "MuseumWaves"
SOUNDBANK = "MuseumCues"


def u8(v): return struct.pack("<B", v)
def u16(v): return struct.pack("<H", v)
def u32(v): return struct.pack("<I", v)
def f32(v): return struct.pack("<f", v)
def s32(v): return struct.pack("<i", v)
def padded(text, size): return text.encode("ascii").ljust(size, b"\0")


def read_wave(name, seconds):
    with wave.open(os.path.join(AUDIO, name + ".wav")) as w:
        assert w.getnchannels() == 1 and w.getsampwidth() == 2, name
        rate = w.getframerate()
        frames = w.readframes(min(w.getnframes(), int(rate * seconds)))
    return rate, frames


# Variables the sound bank's sounds are bound to through RPC (runtime parameter control) curves. accessibility: 0x01 public, 0x04 cue-scoped
# (0x05 = a variable of each cue, set with Cue.SetVariable; 0x01 alone = a global one, set with AudioEngine.SetGlobalVariable).
# name, accessibility, initial, minimum, maximum
VARIABLES = [("Distance", 0x05, 0.0, 0.0, 100.0), ("MasterTrim", 0x01, 1.0, 0.0, 1.0)]
# variable index, parameter (0 = volume, in centibels), curve points (x, y, interpolation: 0 = linear)
RPCS = [(0, 0, [(0.0, 0.0, 0), (100.0, -6000.0, 0)]), (1, 0, [(0.0, -6000.0, 0), (1.0, 0.0, 0)])]
HEADER_XGS = 77


def build_xgs():
    """Returns (file bytes, absolute offset of each RPC curve): the sound bank refers to the curves by those offsets."""
    cat_off = HEADER_XGS
    var_off = cat_off + len(CATEGORIES) * 10
    cat_name_off = var_off + len(VARIABLES) * 13
    cat_names = b"".join(c.encode("ascii") + b"\0" for c in CATEGORIES)
    var_name_off = cat_name_off + len(cat_names)
    var_names = b"".join(v[0].encode("ascii") + b"\0" for v in VARIABLES)
    rpc_off = var_name_off + len(var_names)
    out = b"XGSF" + u16(46) + u16(0) + u16(0) + b"\0" * 8 + u8(3)
    out += u16(len(CATEGORIES)) + u16(len(VARIABLES)) + u16(0) + u16(0) + u16(len(RPCS)) + u16(0) + u16(0)   # categories, variables, blob1, blob2, rpc, dsp preset, dsp parameter counts
    out += u32(cat_off) + u32(var_off) + u32(0) + u32(0) + u32(0) + u32(0) + u32(cat_name_off) + u32(var_name_off) + u32(rpc_off) + u32(0) + u32(0)
    assert len(out) == HEADER_XGS
    for _ in CATEGORIES:
        out += u8(0xFF) + u16(0) + u16(0) + u8(0) + u16(0xFFFF) + u8(0xFF) + u8(0)
    for _, access, initial, low, high in VARIABLES:
        out += u8(access) + f32(initial) + f32(low) + f32(high)
    out += cat_names + var_names
    assert len(out) == rpc_off
    codes = []
    for variable, parameter, points in RPCS:
        codes.append(len(out))
        out += u16(variable) + u8(len(points)) + u16(parameter)
        for x, y, kind in points:
            out += f32(x) + f32(y) + u8(kind)
    return out, codes


def build_xwb(waves):
    header, bank_data, meta = 48, 96, 24
    entries = len(waves)
    meta_len = entries * meta
    data = b"".join(frames for _, frames in waves)
    seg_off = [header, header + bank_data, header + bank_data + meta_len, header + bank_data + meta_len, header + bank_data + meta_len]
    seg_len = [bank_data, meta_len, 0, 0, len(data)]
    out = b"WBND" + u32(1)
    for o, l in zip(seg_off, seg_len):
        out += u32(o) + u32(l)
    out += u32(0) + u32(entries) + padded(WAVEBANK, 64) + u32(meta) + u32(0) + u32(4) + u32(0) + b"\0" * 8
    offset = 0
    for rate, frames in waves:
        fmt = 0 | (1 << 2) | (rate << 5) | (2 << 23) | (1 << 31)   # PCM, one channel, rate, 2 bytes per frame, 16-bit
        out += u32(0) + u32(fmt) + u32(offset) + u32(len(frames)) + u32(0) + u32(0)
        offset += len(frames)
    return out + data


SOUND_FLAG_HAS_RPC = 0x02


def build_xsb(rpc_codes):
    n = len(CUES)
    header, bank_name = 74, 64
    base = header + bank_name
    wavebank_off = base
    sound_off = wavebank_off + 64
    # Each simple sound: flags, category, volume, pitch, priority, length, wave, wave bank, then (with RPC) rpc data length, count, codes.
    sound_size = 12 + 2 + 1 + 4 * len(rpc_codes)
    cue_simple_off = sound_off + sound_size * n
    cue_name_index_off = cue_simple_off + 5 * n
    cue_name_str_off = cue_name_index_off + 6 * n
    out = b"SDBK" + u16(46) + u16(0) + u16(0) + b"\0" * 8 + u8(0)
    out += u16(n) + u16(0) + u16(0) + u16(0) + u8(1) + u16(n) + u16(0) + u16(0)   # simple cues, complex cues, ?, align, wave banks, sounds, name length, ?
    out += s32(cue_simple_off) + s32(-1) + s32(-1) + s32(0) + s32(-1) + s32(0) + s32(wavebank_off) + s32(0) + s32(cue_name_index_off) + s32(sound_off)
    assert len(out) == header
    out += padded(SOUNDBANK, bank_name) + padded(WAVEBANK, 64)
    for i, (_, _, category, _) in enumerate(CUES):
        out += u8(SOUND_FLAG_HAS_RPC) + u16(category) + u8(0xFF) + u16(0) + u8(0) + u16(0) + u16(i) + u8(0)   # one simple sound: category, wave i of bank 0
        out += u16(1 + 4 * len(rpc_codes)) + u8(len(rpc_codes)) + b"".join(u32(c) for c in rpc_codes)         # the RPC curves it follows
    for i in range(n):
        out += u8(0) + u32(sound_off + sound_size * i)                                                          # one simple cue per sound
    name_pos = cue_name_str_off
    names = b""
    for i, (name, _, _, _) in enumerate(CUES):
        out += u32(name_pos) + u16(0)
        names += name.encode("ascii") + b"\0"
        name_pos += len(name) + 1
    return out + names


def main():
    os.makedirs(OUT, exist_ok=True)
    waves = [read_wave(src, seconds) for _, src, _, seconds in CUES]
    xgs, rpc_codes = build_xgs()
    files = {"museum.xgs": xgs, "museum.xwb": build_xwb(waves), "museum.xsb": build_xsb(rpc_codes)}
    for name, data in files.items():
        with open(os.path.join(OUT, name), "wb") as f:
            f.write(data)
        print("wrote", name, len(data), "bytes")


if __name__ == "__main__":
    main()
