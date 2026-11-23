"""Check the LV2 signal path with a real spoken clip and Lilv's offline host."""

import argparse
import math
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import wave

from make_restoration_fixtures import read_voice, write_voice

URI = "https://weikiboy-tech.github.io/voiceforge-lv2/mono"


def read_pcm(path):
    with wave.open(str(path), "rb") as wav:
        if wav.getnchannels() != 1 or wav.getsampwidth() != 2:
            raise ValueError(f"{path.name} is not mono PCM16.")
        return wav.getframerate(), [sample for (sample,) in struct.iter_unpack(
            "<h", wav.readframes(wav.getnframes()))]


def render(bundle_path, source, destination, settings):
    args = ["lv2apply", "-i", str(source), "-o", str(destination)]
    for symbol, value in settings.items():
        args.extend(("-c", symbol, str(value)))
    subprocess.run([*args, URI], check=True, env={**os.environ, "LV2_PATH": str(bundle_path)})
    return read_pcm(destination)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("lv2_path", type=Path, help="Directory containing voiceforge.lv2")
    bundle_path = parser.parse_args().lv2_path.resolve()
    voice, rate = read_voice()
    with tempfile.TemporaryDirectory(prefix="voiceforge-audio-") as folder:
        work = Path(folder)
        source = work / "voice.wav"
        write_voice(source, voice, rate)
        original_rate, original = read_pcm(source)
        renders = {
            "unity": {},
            "attenuated": {"output_gain": -6},
            "compressed": {"comp_enabled": 1, "comp_threshold": -32,
                           "comp_ratio": 4, "comp_attack": 2},
        }
        output = {}
        for name, settings in renders.items():
            actual_rate, samples = render(bundle_path, source, work / f"{name}.wav", settings)
            if actual_rate != original_rate or len(samples) != len(original):
                raise RuntimeError(f"{name}: sample rate or frame count changed.")
            output[name] = samples

    unity_error = max(abs(actual - expected) for actual, expected
                      in zip(output["unity"], original, strict=True))
    expected_gain = 10 ** (-6 / 20)
    gain_error = max(abs(actual - expected * expected_gain) for actual, expected
                     in zip(output["attenuated"], original, strict=True))
    reduction = 10 * math.log10(sum(sample * sample for sample in original) /
                                sum(sample * sample for sample in output["compressed"]))
    if unity_error > 1 or gain_error > 2 or not 3 <= reduction <= 20:
        raise RuntimeError(f"Unexpected voice render: unity error {unity_error}, "
                           f"gain error {gain_error:.2f}, RMS reduction {reduction:.2f} dB.")
    print(f"Speech render: unity error {unity_error} LSB, gain error {gain_error:.2f} LSB, "
          f"compression reduced RMS by {reduction:.2f} dB.")


if __name__ == "__main__":
    main()
