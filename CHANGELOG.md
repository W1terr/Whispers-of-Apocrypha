# Changelog

## 2.2

- Works on Skyrim Special Edition 1.5.97 too. The dark is now found with the game's own light level condition
  (the one the Black Book script uses), so the same plugin runs on SE and AE.

## 1.3

- The madness now comes only from the dark spots of the Black Book levels. The ink water of Apocrypha no longer counts,
  and the eyes that rose from the ink are gone with it.
- Eyes left in a save made with an older version are still cleaned up when it loads.

## 1.2

- Built-in support for **Stress and Fear - A Dynamic Sanity System**. It turns on by itself when Stress and Fear is
  installed; there's nothing extra to install.
  - Stress builds while you're in the dark, faster as the madness deepens: from 0.25 up to 1.5 per second.
  - When Hermaeus Mora's eyes open you get Stress and Fear's own fear reaction: panicked breathing, its fear screen
    effect and +10 stress.
  - When Hermaeus Mora appears, it's a trauma: Stress and Fear's trauma line and +25 stress.
  - Stress and Fear's MCM switches for stress and fear are respected, and its usual stress messages are shown.
- The separate Stress and Fear patch is no longer needed. If it's still installed, the mod detects it and won't add
  stress twice.

## 1.1

- Added a small message interface for other SKSE plugins. It tells them when you're in the dark, when the madness stage
  changes, and when Hermaeus Mora appears. See `src/API.h`.

## 1.0

- First release.
- Staying in the darkness of Apocrypha slowly breaks your mind. The darkness is the dark outside the moving lights of
  the Black Book levels, and the black ink water.
  - **Whispers** (after 1.5 s): voices whisper Hermaeus Mora's words around your head.
  - **The green nightmare** (after 3 s): the screen blurs at the edges and turns sickly green, using one of Skyrim's own
    screen effects.
  - **The eyes** (after 8 s): Hermaeus Mora's faces full of eyes emerge in the dark around you, and eyes rise from the
    ink. Stare at one or walk up to it, and it sinks back into the dark.
  - **The gaze** (after 18 s): Hermaeus Mora himself appears in front of you.
- Step back into the light and everything fades away within 2.4 seconds.
- Uses only the game's own models, textures and screen effects. The whispers are new sounds.
- No settings to configure.
