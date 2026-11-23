"""Verify that Ardour actually exports the recorded voice track through its backend."""
import argparse
import math
import struct
import wave
from pathlib import Path


def read(path):
    with wave.open(str(path), "rb") as wav:
        channels, width, rate, frames = (wav.getnchannels(), wav.getsampwidth(),
                                         wav.getframerate(), wav.getnframes())
        if (channels, width, rate) != (2, 2, 48000):
            raise RuntimeError(f"Unexpected export format: {channels} channels, {width} bytes, {rate} Hz")
        data = [item[0] / 32768 for item in struct.iter_unpack("<h", wav.readframes(frames))]
    if frames < 48000 or max(map(abs, data)) < 0.005:
        raise RuntimeError("Ardour offline export is empty or silent.")
    return data, frames


def main():
    args = argparse.ArgumentParser(description=__doc__)
    args.add_argument("live", type=Path)
    args.add_argument("studio", type=Path)
    paths = args.parse_args()
    live, count = read(paths.live)
    studio, second = read(paths.studio)
    if second != count:
        raise RuntimeError(f"Studio changed the exported duration from {count} to {second} frames.")
    # Exclude the start and end while filters and fades settle.
    lo = 2 * 12000
    hi = min(count * 2 - lo, 2 * 72000)
    reference = sum(x*x for x in live[lo:hi])
    residual = sum((x-y)**2 for x,y in zip(live[lo:hi], studio[lo:hi], strict=True))
    ratio = 10 * math.log10(reference / max(residual, 1e-12))
    if ratio < 28.0:
        raise RuntimeError(f"Studio did not align with Live in Ardour's export: {ratio:.1f} dB.")
    print(f"Ardour exported {count} stereo frames; Live/Studio compensated alignment {ratio:.1f} dB.")


if __name__ == "__main__":
    main()
