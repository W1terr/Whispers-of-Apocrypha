r"""Builds the mod archive "Whispers of Apocrypha <version>.zip": WhispersOfApocrypha.esp, the sounds and
SKSE\Plugins\WhispersOfApocrypha.dll. The version is the target's set_version in xmake.lua (a trailing ".0" is left
out of the name).
usage: python package.py [build dir] [output dir]"""

import re
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "build"
OUTPUT = Path(sys.argv[2]) if len(sys.argv) > 2 else ROOT / "dist"


def version(target: str) -> str:
    text = (ROOT / "xmake.lua").read_text(encoding="utf-8")
    match = re.search(r'target\("' + re.escape(target) + r'"\)\s*\n\s*set_version\("([^"]+)"\)', text)
    full = match.group(1) if match else "0.0.0"
    return re.sub(r"\.0$", "", full)


def write(name: str, files: dict):
    OUTPUT.mkdir(parents=True, exist_ok=True)
    archive = OUTPUT / name
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as zf:
        for entry, path in files.items():
            zf.write(path, entry)
    print(f"wrote {archive} ({archive.stat().st_size / 1024:.0f} KB, {len(files)} files)")


def main():
    plugin = BUILD / "windows" / "x64" / "releasedbg"

    files = {
        "WhispersOfApocrypha.esp": ROOT / "res" / "WhispersOfApocrypha.esp",
        "SKSE/Plugins/WhispersOfApocrypha.dll": plugin / "WhispersOfApocrypha.dll",
    }
    for sound in sorted((ROOT / "res" / "Sound" / "FX" / "WhispersOfApocrypha").glob("*.wav")):
        files[f"Sound/FX/WhispersOfApocrypha/{sound.name}"] = sound
    write(f"Whispers of Apocrypha {version('WhispersOfApocrypha')}.zip", files)


if __name__ == "__main__":
    main()
