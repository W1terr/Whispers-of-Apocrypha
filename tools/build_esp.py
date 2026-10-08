"""Writes WhispersOfApocrypha.esp (ESL flagged): sound descriptors (SNDR) and one image space modifier (IMAD).
No scripts, no edits to vanilla records. The DLL finds them by form id (this plugin's 0x800+).

The madness imod is a copy of Skyrim's DA16DreamState (Vaermina's nightmare: radial blur toward the edges, bloom,
tint) with the tint turned to a sickly green, stronger edge blur and less color. The DLL sets its strength.
Our own whispers live in Sound/FX/WhispersOfApocrypha (tools/make_sounds.py); the other descriptors reuse vanilla
Skyrim / Dawnguard / Dragonborn sound files with a non-positional (2D) output model so the DLL can fade them freely.

usage: python build_esp.py [output.esp]
"""

import struct
import sys
from pathlib import Path

from esm import Plugin

ROOT = Path(__file__).resolve().parent.parent
OUT = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "res" / "WhispersOfApocrypha.esp"
SOUNDS = ROOT / "res" / "Sound" / "FX" / "WhispersOfApocrypha"

OWN = 0x01000000  # this plugin (one master)

# Skyrim.esm forms
CATEGORY_SFX = 0x172A1           # AudioCategorySFX (the "Effects" volume slider)
OUTPUT_MONO_3D = 0xD78B4         # SOMMono01400_verb: positional, reverb, 150 - 1400 units
OUTPUT_STEREO = 0x7EDCA          # SOMStereo: 2D
OUTPUT_STEREO_VERB = 0xD78B8     # SOMStereo_verb: 2D with reverb

DESCRIPTOR_STANDARD = 0x1EEF540A  # BGSStandardSoundDef
LOOP = 0x08

# local form ids (ESL range 0x800 - 0xFFF); keep in sync with src/Sounds.cpp
WHISPER_FIRST = 0x800            # 0x800 + i for whisper_01 ... whisper_NN
BED = 0x880
SOUL_CAIRN = 0x881
WORD_WALL = 0x882
HEARTBEAT = 0x883
GAZE = 0x884
BOOK_HUM = 0x885
RUMBLE = 0x886
MADNESS_IMOD = 0x890


def sub(typ, data):
    if isinstance(data, str):
        data = data.encode("cp1252") + b"\0"
    return typ.encode() + struct.pack("<H", len(data)) + data


def u32(v):
    return struct.pack("<I", v)


def f32(v):
    return struct.pack("<f", v)


def record(typ, formid, subs, flags=0):
    body = b"".join(subs)
    return typ.encode() + struct.pack("<IIIIHH", len(body), flags, formid, 0, 44, 0) + body


def group(label, contents):
    body = b"".join(contents)
    return b"GRUP" + struct.pack("<I", 24 + len(body)) + label.encode() + struct.pack("<iHHHH", 0, 0, 0, 0, 0) + body


def sndr(local, edid, files, output, loop=False, priority=128, attenuation_db=0.0):
    subs = [sub("EDID", edid), sub("CNAM", u32(DESCRIPTOR_STANDARD)), sub("GNAM", u32(CATEGORY_SFX))]
    subs += [sub("ANAM", f) for f in files]
    subs += [
        sub("ONAM", u32(output)),
        sub("LNAM", bytes([0, LOOP if loop else 0, 0, 0])),
        # frequency shift, frequency variance, priority, db variance, static attenuation (1/100 dB)
        sub("BNAM", struct.pack("<bbBBH", 0, 0, priority, 0, int(attenuation_db * 100))),
    ]
    return record("SNDR", OWN | local, subs)


def keys(*pairs):
    return b"".join(struct.pack("<2f", t, v) for t, v in pairs)


def madness_imod():
    """DA16DreamState with a green tint: non-animatable, so the time 0 keys hold while it is applied."""
    source = Plugin("Skyrim.esm").by_edid["da16dreamstate"]
    replace = {
        "TNAM": struct.pack("<5f", 0.0, 0.22, 0.85, 0.30, 0.6) + struct.pack("<5f", 1.0, 0.22, 0.85, 0.30, 0.6),
        "RNAM": keys((0.0, 0.35), (1.0, 0.35)),   # radial blur strength
        "UNAM": keys((0.0, 0.3), (1.0, 0.3)),     # radial blur start (from the center)
        "BNAM": keys((0.0, 0.15), (1.0, 0.15)),   # blur radius
        "IAD": keys((0.0, 0.55), (1.0, 0.55)),  # cinematic saturation multiplier
    }
    subs = [sub("EDID", "WOA_MadnessImod")]
    for typ, data in source.subs():
        if typ == "EDID":
            continue
        subs.append(typ.encode("latin-1") + struct.pack("<H", len(replace.get(typ, data))) + replace.get(typ, data))
    return record("IMAD", OWN | MADNESS_IMOD, subs)


def main():
    whispers = sorted(SOUNDS.glob("whisper_*.wav"))
    if not whispers:
        sys.exit("no whispers: run make_sounds.py first")
    ours = "Data\\Sound\\FX\\WhispersOfApocrypha\\"
    records = [
        sndr(WHISPER_FIRST + i, f"WOA_Whisper{i + 1:02d}", [ours + w.name], OUTPUT_MONO_3D, priority=160)
        for i, w in enumerate(whispers)
    ]
    records += [
        sndr(BED, "WOA_WhisperBedLP", [ours + "bed_lp.wav"], OUTPUT_STEREO, loop=True),
        sndr(SOUL_CAIRN, "WOA_SoulCairnWhispersLP",
             ["Data\\Sound\\FX\\DLC01\\AMB\\AMB_Steam_SoulCrack_Whispers_LP.wav"], OUTPUT_STEREO, loop=True),
        sndr(WORD_WALL, "WOA_WordWallWhispersLP",
             ["\\Data\\Sound\\FX\\QST\\QST_WordWall_Whisper_2D_LP.wav"], OUTPUT_STEREO, loop=True),
        sndr(HEARTBEAT, "WOA_HeartbeatLP",
             ["\\Data\\Sound\\FX\\UI\\Health\\Heartbeat\\UI_Health_Heartbeat_A_LP.wav"], OUTPUT_STEREO, loop=True),
        sndr(GAZE, "WOA_Gaze", [ours + "gaze.wav"], OUTPUT_STEREO_VERB, priority=200),
        sndr(BOOK_HUM, "WOA_BlackBookHum",
             [f"data\\Sound\\FX\\DLC02\\QST\\BlackBookHum\\QST_Black_Book_Hum_2D_0{i}.wav" for i in range(1, 5)],
             OUTPUT_STEREO),
        sndr(RUMBLE, "WOA_ApocryphaRumble",
             [f"Data\\Sound\\FX\\DLC02\\AMBr\\Apocrypha\\Random\\AMBr_ApocryphaRandom_2D_0{i}.wav" for i in range(1, 5)],
             OUTPUT_STEREO),
    ]
    body = group("IMAD", [madness_imod()]) + group("SNDR", records)
    count = len(records) + 1 + 2  # records + groups
    header = record("TES4", 0, [
        sub("HEDR", f32(1.71) + struct.pack("<i", count) + u32(0x900)),
        sub("CNAM", "W1ter"),
        sub("SNAM", "Whispers of Apocrypha"),
        sub("MAST", "Skyrim.esm"), sub("DATA", b"\0" * 8),
    ], flags=0x200)  # ESL
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_bytes(header + body)
    print(f"wrote {OUT} ({len(records)} sound descriptors, {len(whispers)} whispers, 1 imagespace modifier)")


if __name__ == "__main__":
    main()
