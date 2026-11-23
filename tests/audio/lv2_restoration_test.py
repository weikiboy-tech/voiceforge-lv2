"""Measure restoration against aligned clean speech and isolated defects."""

import argparse
import math
from pathlib import Path
import tempfile

from lv2_speech_test import read_pcm, render
from make_restoration_fixtures import corrupted, read_voice, write_voice


def improvement(before, after):
    return 10 * math.log10(max(before, 1) / max(after, 1))


def energy(signal):
    return sum(x * x for x in signal)


def difference(heard, reference):
    return [x - y for x, y in zip(heard, reference, strict=True)]


def bass(signal, rate):
    coefficient = -math.expm1(-math.tau * 120 / rate)
    low = 0.0
    for sample in signal:
        low += coefficient * (sample - low)
        yield low


def studio_delay(rate):
    size = 256
    while size < 8192 and size < rate * 1024 / 48000:
        size *= 2
    return size + math.ceil(rate * 0.00065) + 2


def main():
    args = argparse.ArgumentParser(description=__doc__)
    args.add_argument("lv2_path", type=Path)
    path = args.parse_args().lv2_path.resolve()
    voice, rate = read_voice()
    defects, clicks, plosive = corrupted(voice, rate)
    settings = {
        "clicks": {"mode": 1, "de_click_enabled": 1},
        "plosive": {"de_plosive_enabled": 1},
        "noise": {"mode": 1, "noise_enabled": 1},
    }
    with tempfile.TemporaryDirectory(prefix="voiceforge-restoration-") as directory:
        workspace = Path(directory)
        clean = workspace / "clean.wav"
        write_voice(clean, voice, rate)
        _, reference = read_pcm(clean)
        raw = {}
        restored = {}
        for name, defect in defects.items():
            corrupt = workspace / f"{name}.wav"
            write_voice(corrupt, (x + y for x, y in zip(voice, defect, strict=True)), rate)
            _, original = read_pcm(corrupt)
            new_rate, samples = render(path, corrupt, workspace / f"{name}-out.wav", settings[name])
            if new_rate != rate or len(samples) != len(reference):
                raise RuntimeError(f"Invalid render length or sample rate for {name}.")
            delay = studio_delay(rate) if settings[name].get("mode") else 0
            count = len(samples) - delay
            raw[name] = difference(original[:count], reference[:count])
            restored[name] = difference(samples[delay:], reference[:count])

    click_indices = {i for start in clicks for i in range(start, start + 4)}
    click_gain = improvement(energy(raw["clicks"][i] for i in click_indices),
                             energy(restored["clicks"][i] for i in click_indices))
    outside_click = improvement(energy(reference),
                                energy(x for i, x in enumerate(restored["clicks"]) if i not in click_indices))

    begin, end = plosive
    plosive_gain = improvement(energy(list(bass(raw["plosive"], rate))[begin:end]),
                               energy(list(bass(restored["plosive"], rate))[begin:end]))
    speech_after = slice(end + rate // 4, len(reference) - rate)
    plosive_after = improvement(energy(reference[speech_after]), energy(restored["plosive"][speech_after]))

    quiet = slice(rate // 2, rate * 9 // 10)
    noise_gain = improvement(energy(raw["noise"][quiet]), energy(restored["noise"][quiet]))
    speech = slice(rate, len(restored["noise"]) - rate // 2)
    noise_speech = improvement(energy(reference[speech]), energy(restored["noise"][speech]))

    print(f"De-Click: {click_gain:.2f} dB defect improvement, {outside_click:.2f} dB speech-to-error outside clicks")
    print(f"De-Plosive: {plosive_gain:.2f} dB bass error improvement, {plosive_after:.2f} dB speech-to-error after burst")
    print(f"Noise: {noise_gain:.2f} dB pause reduction, {noise_speech:.2f} dB speech-to-error in speech section")
    if click_gain < 9 or outside_click < 36 or plosive_gain < 2 or plosive_after < 25:
        raise RuntimeError("Restoration failed the transient suppression or preservation thresholds.")
    if noise_gain < 6 or noise_speech < 15:
        raise RuntimeError("Noise suppression failed the residual-noise or speech preservation threshold.")


if __name__ == "__main__":
    main()
