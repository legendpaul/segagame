"""Build original 14 kHz mono PCM audio for the Mega Drive sound mix.

The samples are deliberately synthesised from noise, filtered transients and
formant-like tone clusters.  This keeps the game self-contained and gives the
stadium a much more natural texture than a flat PSG noise channel without
shipping or imitating any copyrighted recording.
"""

from __future__ import annotations

import math
import random
import wave
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "res" / "audio"
RATE = 14000
RNG = random.Random(0xD0D6E)


def clamp(value: float) -> int:
    return max(-32767, min(32767, int(value * 32767)))


def write(name: str, samples: list[float]) -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    with wave.open(str(OUT / name), "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(RATE)
        payload = bytearray()
        for sample in samples:
            payload.extend(clamp(sample).to_bytes(2, "little", signed=True))
        wav.writeframes(payload)


def env(t: float, duration: float, attack: float = 0.05,
        release: float = 0.25) -> float:
    rise = min(1.0, t / max(attack, 0.001))
    fall = min(1.0, (duration - t) / max(release, 0.001))
    return max(0.0, min(rise, fall))


def coloured_noise(count: int, warmth: float = 0.94) -> list[float]:
    """Low-pass white noise into the broad spectrum of a distant crowd."""
    out: list[float] = []
    low = 0.0
    mid = 0.0
    for _ in range(count):
        white = RNG.uniform(-1.0, 1.0)
        low = low * warmth + white * (1.0 - warmth)
        mid = mid * 0.72 + white * 0.28
        out.append(low * 1.8 + mid * 0.35 + white * 0.06)
    return out


def crowd(duration: float, intensity: float, swell: bool) -> list[float]:
    count = int(duration * RATE)
    noise = coloured_noise(count, 0.955)
    voices = []
    phases = [RNG.random() * math.tau for _ in range(18)]
    freqs = [RNG.uniform(82.0, 235.0) for _ in phases]
    for i in range(count):
        t = i / RATE
        murmur = 0.0
        for n, (phase, freq) in enumerate(zip(phases, freqs)):
            wobble = 1.0 + 0.018 * math.sin(math.tau * (0.7 + n * 0.031) * t)
            murmur += math.sin(phase + math.tau * freq * wobble * t)
        murmur /= len(phases)
        shape = env(t, duration, 0.12 if swell else 0.03,
                    0.55 if swell else 0.08)
        if swell:
            shape *= 0.40 + 0.60 * min(1.0, t / max(0.28, duration * 0.18))
        flutter = 0.90 + 0.10 * math.sin(math.tau * 5.2 * t)
        voices.append((noise[i] * 0.72 + murmur * 0.55) *
                      shape * flutter * intensity)
    return voices


def chant() -> list[float]:
    duration = 1.62
    count = int(duration * RATE)
    noise = coloured_noise(count, 0.97)
    out = []
    # Four broad "oh" pulses. Detuned fundamentals and formants make it read
    # as a group chant rather than a single square-wave note.
    starts = (0.04, 0.40, 0.80, 1.16)
    for i in range(count):
        t = i / RATE
        value = noise[i] * 0.12
        for beat, start in enumerate(starts):
            local = t - start
            if 0.0 <= local < 0.32:
                e = math.sin(math.pi * local / 0.32) ** 0.65
                base = 116.0 if beat < 2 else 109.0
                vowel = 0.0
                for detune in (-0.026, -0.013, 0.0, 0.017, 0.031):
                    f = base * (1.0 + detune)
                    vowel += (math.sin(math.tau * f * local) * 0.23 +
                              math.sin(math.tau * f * 2.0 * local) * 0.12 +
                              math.sin(math.tau * f * 5.8 * local) * 0.055)
                value += vowel * e
        out.append(value * env(t, duration, 0.02, 0.12) * 0.52)
    return out


def whoop() -> list[float]:
    duration = 0.36
    count = int(duration * RATE)
    noise = coloured_noise(count, 0.91)
    out = []
    for i in range(count):
        t = i / RATE
        e = env(t, duration, 0.025, 0.18)
        voices = sum(math.sin(math.tau * (190 + n * 23 + 120 * t) * t + n)
                     for n in range(7)) / 7
        out.append((noise[i] * 0.45 + voices * 0.55) * e * 0.60)
    return out


def whoosh() -> list[float]:
    duration = 0.19
    count = int(duration * RATE)
    noise = coloured_noise(count, 0.64)
    return [noise[i] * env(i / RATE, duration, 0.01, 0.13) *
            (0.35 + 0.65 * i / count) * 0.72 for i in range(count)]


def thump(duration: float, start_hz: float, end_hz: float,
          noise_amount: float) -> list[float]:
    count = int(duration * RATE)
    noise = coloured_noise(count, 0.65)
    phase = 0.0
    out = []
    for i in range(count):
        t = i / RATE
        progress = t / duration
        frequency = start_hz + (end_hz - start_hz) * progress
        phase += math.tau * frequency / RATE
        e = math.exp(-7.5 * progress)
        out.append((math.sin(phase) * (1.0 - noise_amount) +
                    noise[i] * noise_amount) * e * 0.92)
    return out


def whistle() -> list[float]:
    duration = 0.48
    count = int(duration * RATE)
    out = []
    for i in range(count):
        t = i / RATE
        e = env(t, duration, 0.012, 0.10)
        vibrato = 1.0 + 0.012 * math.sin(math.tau * 14.0 * t)
        tone = math.sin(math.tau * 2320.0 * vibrato * t)
        tone += 0.28 * math.sin(math.tau * 4640.0 * vibrato * t)
        out.append(tone * e * 0.56)
    return out


def main() -> None:
    write("crowd_bed.wav", crowd(1.12, 0.30, False))
    write("crowd_chant.wav", chant())
    write("crowd_throw.wav", whoop())
    write("crowd_elimination.wav", crowd(1.05, 0.62, True))
    write("crowd_round.wav", crowd(1.90, 0.78, True))
    write("crowd_gameover.wav", crowd(3.20, 0.94, True))
    write("throw.wav", whoosh())
    write("pickup.wav", thump(0.11, 410.0, 245.0, 0.24))
    write("hit.wav", thump(0.28, 150.0, 54.0, 0.36))
    write("bounce.wav", thump(0.14, 310.0, 145.0, 0.17))
    write("whistle.wav", whistle())
    print(f"Wrote 11 original PCM assets to {OUT}")


if __name__ == "__main__":
    main()
