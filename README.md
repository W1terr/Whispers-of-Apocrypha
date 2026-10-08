# Whispers of Apocrypha

A Lovecraft-inspired SKSE plugin for Skyrim Anniversary Edition (Dragonborn). The darkness of Apocrypha slowly breaks
your mind: the dark outside the moving lights of the Black Book levels (where the vanilla game drains your health while
your light level is below 30) and the black ink water.

The longer you stay in the dark, the deeper it goes:

1. **Whispers** (after 1.5 s): voices whisper Hermaeus Mora's words around your head, over a bed of murmuring and the
   Soul Cairn whispers.
2. **The green nightmare** (after 3 s): Skyrim's own `DA16DreamState` image space (Vaermina's nightmare: blurred edges,
   bloom) turned sickly green, growing with your madness. The word wall chant joins the whispers.
3. **The eyes** (after 8 s): Hermaeus Mora's faces full of eyes, the ones from the walls of Apocrypha, emerge in the
   dark around you, and eyes rise from the ink. Stare at one, or walk up to it, and it sinks back into the dark. Your
   heart starts to pound.
4. **The gaze** (after 18 s): Hermaeus Mora himself appears in front of you and tells you he sees you.

Step back into the light and it all fades away within 2.4 seconds.

Everything you see is from the game itself: no new meshes or textures. The whispers are new sounds.

## Requirements

- Skyrim Anniversary Edition (1.6.1130+ / 1.7) with Dragonborn
- [SKSE](https://skse.silverlock.org/)
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444)

## Installation

Install the archive with a mod manager. It contains `WhispersOfApocrypha.esp` (ESL flagged: sound descriptors and the
green image space), `SKSE/Plugins/WhispersOfApocrypha.dll` and the sounds in `Sound/FX/WhispersOfApocrypha`. There are
no settings to change.

## Build

- `build.bat` builds the plugin with xmake and [CommonLibSSE-NG](https://github.com/alandtse/CommonLibVR/tree/ng)
  (`lib/commonlibsse-ng`, a git submodule). The output folder can be set in a `build.local.bat` (not in git).
- `tools/make_sounds.py` makes the whispers from Windows text to speech (needs numpy and scipy).
- `tools/build_esp.py` writes `res/WhispersOfApocrypha.esp` (reads `Skyrim.esm` for the image space it copies).
- `tools/package.py <build dir> <output dir>` makes the mod archive.
- `tools/make_banner.py` makes the mod page image from a screenshot and the game's eye texture.

## License

GPL-3.0
