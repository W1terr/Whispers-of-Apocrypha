r"""Builds the mod archive: WhispersOfApocrypha.esp, the sounds, SKSE\Plugins\WhispersOfApocrypha.dll.
usage: python package.py [build dir] [output dir]"""

import re
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "build"
OUTPUT = Path(sys.argv[2]) if len(sys.argv) > 2 else ROOT / "dist"


def version() -> str:
    match = re.search(r'set_version\("([^"]+)"\)', (ROOT / "xmake.lua").read_text(encoding="utf-8"))
    return match.group(1) if match else "0.0.0"


def main():
    plugin = BUILD / "windows" / "x64" / "releasedbg"
    files = {
        "WhispersOfApocrypha.esp": ROOT / "res" / "WhispersOfApocrypha.esp",
        "SKSE/Plugins/WhispersOfApocrypha.dll": plugin / "WhispersOfApocrypha.dll",
    }
    for sound in sorted((ROOT / "res" / "Sound" / "FX" / "WhispersOfApocrypha").glob("*.wav")):
        files[f"Sound/FX/WhispersOfApocrypha/{sound.name}"] = sound
    OUTPUT.mkdir(parents=True, exist_ok=True)
    archive = OUTPUT / f"Whispers of Apocrypha {version()}.zip"
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as zf:
        for name, path in files.items():
            zf.write(path, name)
    print(f"wrote {archive} ({archive.stat().st_size / 1024:.0f} KB, {len(files)} files)")


if __name__ == "__main__":
    main()
