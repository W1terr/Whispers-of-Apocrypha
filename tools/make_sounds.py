"""Generates the mod's whisper sounds: Windows text-to-speech phrases turned into reverberant multi-voice whispers.

Every phrase is spoken by a SAPI voice (System.Speech, part of Windows), then "whisperized": the short-time spectrum
keeps its magnitude but gets random phase, so the voiced pitch is replaced by breath noise while the words stay
(barely) intelligible. Three such whispers with shifted formants and small delays are mixed so it sounds like many
mouths speaking at once, then a long dark reverb is added. Some files are reversed babble.

Output (res/Sound/FX/WhispersOfApocrypha):
  whisper_NN.wav   mono one-shots, played 3D around the player by the DLL
  bed_lp.wav       stereo whisper bed loop (seamless)
  gaze.wav         mono stinger for the moment Hermaeus Mora looks at you (reversed reverb swell + drone)

usage: python make_sounds.py [--gaze]   (needs numpy + scipy; --gaze: only gaze.wav)
"""

import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np
from scipy import signal
from scipy.io import wavfile

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "res" / "Sound" / "FX" / "WhispersOfApocrypha"
RATE = 44100
RNG = np.random.default_rng(0xA90C)

# (text, voice, speaking rate, reversed)
PHRASES = [
    ("Knowledge... is power.", "David", -3, False),
    ("All things are known to me.", "David", -3, False),
    ("You belong to me.", "Zira", -3, False),
    ("Deeper. Come deeper.", "David", -4, False),
    ("I see you.", "Zira", -4, False),
    ("Do not look away.", "David", -3, False),
    ("Your mind is mine.", "Zira", -3, False),
    ("The pages remember you.", "David", -3, False),
    ("Stay. Stay forever.", "Zira", -4, False),
    ("We are watching.", "David", -3, False),
    ("Forget the sun.", "Zira", -3, False),
    ("That is not dead which can eternal lie.", "David", -3, False),
    ("Read... and be read.", "Zira", -3, False),
    ("The ink hungers.", "David", -4, False),
    ("There is no way out of knowing.", "Zira", -3, False),
    ("Hermaeus Mora sees all.", "David", -3, False),
    ("Your secrets taste sweet.", "Zira", -3, False),
    ("Listen.", "David", -4, False),
    ("All things are known to me.", "Zira", -2, True),
    ("You belong to me. You belong to me.", "David", -1, True),
    ("Knowledge is power, and power is mine.", "Zira", -1, True),
    ("Deeper into the dark, deeper into the ink.", "David", -1, True),
]

GAZE_PHRASE = ("I see you.", "David", -5)


def speak(text: str, voice: str, rate: int, path: Path):
    script = (
        "Add-Type -AssemblyName System.Speech;"
        "$s = New-Object System.Speech.Synthesis.SpeechSynthesizer;"
        f"$s.SelectVoice('Microsoft {voice} Desktop');"
        f"$s.Rate = {rate};"
        f"$f = New-Object System.Speech.AudioFormat.SpeechAudioFormatInfo({RATE}, "
        "[System.Speech.AudioFormat.AudioBitsPerSample]::Sixteen, [System.Speech.AudioFormat.AudioChannel]::Mono);"
        f"$s.SetOutputToWaveFile('{path}', $f);"
        f"$s.Speak(\"{text}\");"
        "$s.Dispose()"
    )
    subprocess.run(["powershell", "-NoProfile", "-Command", script], check=True)
    rate_, data = wavfile.read(path)
    assert rate_ == RATE
    data = data.astype(np.float64) / 32768.0
    return trim(data)


def trim(x, threshold=0.004):
    idx = np.nonzero(np.abs(x) > threshold)[0]
    if len(idx) == 0:
        return x
    return x[max(0, idx[0] - 400):idx[-1] + 400]


def resample(x, factor):
    """Plays x 'factor' times as fast (factor < 1: slower and lower formants)."""
    n = int(len(x) / factor)
    return signal.resample(x, n)


def whisperize(x, nperseg=1024):
    f, t, z = signal.stft(x, RATE, nperseg=nperseg, noverlap=nperseg * 3 // 4)
    mag = np.abs(z)
    # tilt the spectrum toward breath: weaken the low (voiced) band, lift the sibilant band
    tilt = np.clip((f / 1500.0) ** 0.6, 0.25, 1.6)[:, None]
    phase = RNG.uniform(-np.pi, np.pi, size=z.shape)
    _, y = signal.istft(mag * tilt * np.exp(1j * phase), RATE, nperseg=nperseg, noverlap=nperseg * 3 // 4)
    return y


def bandpass(x, lo=160.0, hi=7500.0):
    sos = signal.butter(4, [lo, hi], btype="band", fs=RATE, output="sos")
    return signal.sosfilt(sos, x)


def lowpass(x, hi):
    sos = signal.butter(4, hi, btype="low", fs=RATE, output="sos")
    return signal.sosfilt(sos, x)


def reverb(x, seconds=2.4, decay=0.55, wet=0.4):
    n = int(seconds * RATE)
    t = np.arange(n) / RATE
    ir = RNG.standard_normal(n) * np.exp(-t / decay)
    ir = lowpass(ir, 4500.0)
    ir[: int(0.012 * RATE)] *= np.linspace(0.0, 1.0, int(0.012 * RATE))
    ir /= np.sqrt(np.sum(ir ** 2))
    tail = signal.fftconvolve(x, ir)
    dry = np.concatenate([x, np.zeros(len(tail) - len(x))])
    return dry * (1.0 - wet) + tail * wet


def legion(x):
    """Three whispering mouths: shifted formants, small delays."""
    voices = []
    for factor, delay, gain in ((0.86, 0.0, 1.0), (0.95, 0.035, 0.7), (1.07, 0.07, 0.55)):
        v = whisperize(resample(x, factor)) * gain
        v = np.concatenate([np.zeros(int(delay * RATE)), v])
        voices.append(v)
    n = max(len(v) for v in voices)
    return sum(np.pad(v, (0, n - len(v))) for v in voices)


def fade(x, fade_in=0.02, fade_out=0.35):
    a, b = int(fade_in * RATE), int(fade_out * RATE)
    y = x.copy()
    y[:a] *= np.linspace(0, 1, a)
    y[-b:] *= np.linspace(1, 0, b) ** 2
    return y


def normalize(x, peak_db=-3.0):
    peak = np.max(np.abs(x))
    return x * (10 ** (peak_db / 20) / peak) if peak > 0 else x


def write(path: Path, x):
    data = np.clip(x, -1.0, 1.0)
    wavfile.write(path, RATE, (data * 32767).astype(np.int16))
    print(f"  {path.name}: {len(data) / RATE:.2f} s")


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    if "--gaze" in sys.argv:  # only the gaze stinger
        make_gaze(Path(tempfile.mkdtemp(prefix="woa_tts_")))
        return
    for old in OUT.glob("*.wav"):
        old.unlink()
    tmp = Path(tempfile.mkdtemp(prefix="woa_tts_"))
    dry_whispers = []
    for i, (text, voice, rate, backwards) in enumerate(PHRASES, 1):
        speech = speak(text, voice, rate, tmp / f"p{i}.wav")
        w = bandpass(legion(speech))
        if backwards:
            w = w[::-1]
        dry_whispers.append(w)
        write(OUT / f"whisper_{i:02d}.wav", normalize(fade(reverb(w))))

    # stereo bed: a crowd of quiet whispers scattered over 24 s, wrapped into a seamless loop
    length = int(24 * RATE)
    bed = np.zeros((length, 2))
    for _ in range(46):
        w = dry_whispers[RNG.integers(len(dry_whispers))]
        if RNG.random() < 0.5:
            w = w[::-1]
        w = reverb(w, wet=0.6) * RNG.uniform(0.25, 0.7)
        start = int(RNG.integers(length))
        pan = RNG.uniform(0.1, 0.9)
        for ch, g in ((0, np.sqrt(1 - pan)), (1, np.sqrt(pan))):
            idx = (start + np.arange(len(w))) % length  # wraps around: the loop has no seam
            np.add.at(bed[:, ch], idx, w * g)
    bed /= np.max(np.abs(bed))
    bed *= 10 ** (-4 / 20)
    wavfile.write(OUT / "bed_lp.wav", RATE, (bed * 32767).astype(np.int16))
    print(f"  bed_lp.wav: {length / RATE:.2f} s stereo")

    make_gaze(tmp)


def make_gaze(tmp: Path):
    """Hermaeus Mora's "I see you": a reversed reverb swell into the whispered words, over a low beating drone."""
    text, voice, rate = GAZE_PHRASE
    w = bandpass(legion(speak(text, voice, rate, tmp / "gaze.wav")))
    swell = reverb(w[::-1], seconds=3.5, decay=1.1, wet=1.0)[::-1]
    swell = swell[-int(3.2 * RATE):]
    words = reverb(w, seconds=2.5, decay=0.7, wet=0.35)
    t = np.arange(int(6.0 * RATE)) / RATE
    env = np.clip(t / 2.5, 0, 1) ** 2 * np.clip((6.0 - t) / 2.0, 0, 1)
    drone = (np.sin(2 * np.pi * 41 * t) + 0.7 * np.sin(2 * np.pi * 43.5 * t) + 0.35 * np.sin(2 * np.pi * 82.5 * t)) * env
    rumble = lowpass(RNG.standard_normal(len(t)), 120.0) * env * 2.0
    gaze = np.zeros(len(t))
    gaze[: len(swell)] += normalize(swell, -6)
    gaze[len(swell): len(swell) + len(words)] += normalize(words, -3)[: len(t) - len(swell)]
    gaze += normalize(drone + rumble, -9)
    write(OUT / "gaze.wav", normalize(fade(gaze, 0.05, 0.8), -1.5))


if __name__ == "__main__":
    sys.exit(main())
