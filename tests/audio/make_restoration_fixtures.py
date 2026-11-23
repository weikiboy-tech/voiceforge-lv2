"""Generate aligned speech references with deterministic restoration defects."""

import argparse
import math
from pathlib import Path
import random
import struct
import wave

SOURCE = Path(__file__).resolve().parents[1] / "fixtures" / "thorsten-neutral.wav"


def read_voice():
    with wave.open(str(SOURCE), "rb") as wav:
        if wav.getnchannels() != 1 or wav.getsampwidth() != 2:
            raise ValueError("The fixture must be 16-bit mono PCM.")
        pcm = wav.readframes(wav.getnframes())
        rate = wav.getframerate()
    voice = [sample / 32768 for (sample,) in struct.iter_unpack("<h", pcm)]
    peak = max(map(abs, voice))
    if peak <= 0 or len(voice) < 2 * rate:
        raise ValueError("The fixture must contain at least two seconds of speech.")
    return [0.0] * rate + [sample * (0.55 / peak) for sample in voice] + [0.0] * rate, rate


def write_voice(path, signal, rate):
    pcm = bytearray()
    for sample in signal:
        if not -1.0 <= sample < 1.0:
            raise ValueError(f"Fixture would clip in {path.name}.")
        pcm.extend(struct.pack("<h", round(sample * 32767)))
    with wave.open(str(path), "wb") as wav:
        wav.setparams((1, 2, rate, 0, "NONE", "not compressed"))
        wav.writeframes(pcm)


def corrupted(reference, rate):
    generator = random.Random(17)
    noise = []
    for index in range(len(reference)):
        t = index / rate
        fan = 0.009 * math.sin(math.tau * 120 * t) + 0.003 * math.sin(math.tau * 240 * t)
        noise.append(fan + generator.gauss(0.0, 0.004))

    clicks = [0.0] * len(reference)
    click_samples = [round(t * rate) for t in (0.35, 1.38, 2.18)]
    for start in click_samples:
        clicks[start:start + 4] = [0.34, -0.22, 0.13, -0.07]

    plosive = [0.0] * len(reference)
    plosive_start = round(1.65 * rate)
    plosive_length = round(0.11 * rate)
    for index in range(plosive_length):
        t = index / rate
        progress = index / plosive_length
        window = math.sin(math.pi * progress) ** 2
        plosive[plosive_start + index] = 0.36 * window * math.sin(math.tau * 52 * t)

    return {
        "noise": noise, "clicks": clicks, "plosive": plosive
    }, click_samples, (plosive_start, plosive_start + plosive_length)


def main():
    args = argparse.ArgumentParser(description=__doc__)
    args.add_argument("output", type=Path)
    output = args.parse_args().output
    output.mkdir(parents=True, exist_ok=True)
    reference, rate = read_voice()
    defects, click_samples, plosive = corrupted(reference, rate)
    write_voice(output / "clean.wav", reference, rate)
    for name, defect in defects.items():
        write_voice(output / f"{name}.wav",
                    (speech + added for speech, added in zip(reference, defect, strict=True)), rate)
    print(f"{rate} Hz, {len(reference)} frames")
    print(f"Noise-only interval: [0, {rate})")
    print(f"Click start samples: {click_samples}, each 4 frames")
    print(f"Plosive interval: [{plosive[0]}, {plosive[1]})")


if __name__ == "__main__":
    main()
