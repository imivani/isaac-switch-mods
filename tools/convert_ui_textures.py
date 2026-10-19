"""Lossless PNG -> the game's four-plane RGBA PCX texture format.

The existing Switch ANM2 files name PNG sheets while the dumped textures are
stored as PCX. Preserve those logical ANM2 paths, and stage a matching .pcx.
No game files are inputs to the distributed resource package.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct

from PIL import Image


def encode_pcx(image: Image.Image) -> bytes:
    image = image.convert("RGBA")
    width, height = image.size
    if not (1 <= width <= 8192 and 1 <= height <= 8192):
        raise ValueError("Unexpected UI texture dimensions")
    stride = (width + 1) & ~1
    header = bytearray(128)
    header[:4] = b"\x0a\x05\x01\x08"
    struct.pack_into("<6H", header, 4, 0, 0, width - 1, height - 1, width, height)
    # The Switch loader uses byte 64 as a premultiplied-alpha flag. PNG pixels
    # are straight alpha, so leave 0 and let the engine convert them normally.
    # Dumped game textures use 1 because their pixels are already premultiplied.
    header[64] = 0
    header[65] = 4
    struct.pack_into("<HH", header, 66, stride, 1)
    pixels = image.tobytes()
    output = bytearray(header)
    for y in range(height):
        row = pixels[y * width * 4:(y + 1) * width * 4]
        for channel in range(4):
            plane = row[channel::4] + b"\x00" * (stride - width)
            pos = 0
            while pos < stride:
                value, run = plane[pos], 1
                while pos + run < stride and run < 63 and plane[pos + run] == value:
                    run += 1
                if run > 1 or value >= 0xc0:
                    output.extend((0xc0 | run, value))
                else:
                    output.append(value)
                pos += run
    return bytes(output)


def decode_pcx(data: bytes) -> Image.Image:
    if len(data) < 128 or data[:4] != b"\x0a\x05\x01\x08" or data[65] != 4:
        raise ValueError("Expected four-plane Switch PCX")
    xmin, ymin, xmax, ymax = struct.unpack_from("<4H", data, 4)
    width, height = xmax - xmin + 1, ymax - ymin + 1
    stride = struct.unpack_from("<H", data, 66)[0]
    if not (1 <= width <= stride <= 8192 and 1 <= height <= 8192):
        raise ValueError("Invalid PCX bounds")
    count = height * 4 * stride
    decoded = bytearray()
    position = 128
    while len(decoded) < count:
        if position >= len(data):
            raise ValueError("Truncated PCX pixels")
        value, run = data[position], 1
        position += 1
        if value >= 0xc0:
            run = value & 0x3f
            if not run or position >= len(data):
                raise ValueError("Invalid PCX run")
            value = data[position]
            position += 1
        if len(decoded) + run > count:
            raise ValueError("PCX run exceeds image")
        decoded.extend(bytes((value,)) * run)
    if position != len(data):
        raise ValueError("Unexpected trailing PCX data")
    pixels = bytearray(width * height * 4)
    for y in range(height):
        for channel in range(4):
            begin = (y * 4 + channel) * stride
            pixels[y * width * 4 + channel:(y + 1) * width * 4:4] = decoded[begin:begin + width]
    return Image.frombytes("RGBA", (width, height), bytes(pixels))


def stage(source: Path, destination: Path, *, extra_copy_extensions: tuple[str, ...] = ()) -> list[dict]:
    source, destination = source.resolve(), destination.resolve()
    if not source.is_dir() or source == destination or destination.is_relative_to(source):
        raise ValueError("Use an existing input tree and a separate output tree")
    records = []
    paths = set()
    for path in sorted(source.rglob("*")):
        if not path.is_file():
            continue
        relative = path.relative_to(source)
        if path.suffix.lower() not in (".png", ".anm2", ".fnt") + extra_copy_extensions:
            raise ValueError(f"Unexpected asset in resource tree: {relative}")
        raw = path.read_bytes()
        extra = {}
        if path.suffix.lower() == ".png":
            with Image.open(path) as image:
                rgba = image.convert("RGBA")
                encoded = encode_pcx(rgba)
                decoded = decode_pcx(encoded)
                if decoded.size != rgba.size or decoded.tobytes() != rgba.tobytes():
                    raise ValueError(f"Texture pixel verification failed: {relative}")
                extra = {"width": rgba.width, "height": rgba.height,
                         "rgba_sha256": hashlib.sha256(rgba.tobytes()).hexdigest()}
            output_relative = relative.with_suffix(".pcx")
        else:
            encoded, output_relative = raw, relative
        key = output_relative.as_posix().lower()
        if key in paths:
            raise ValueError(f"Conflicting output texture name: {key}")
        paths.add(key)
        output = (destination / output_relative).resolve()
        if not output.is_relative_to(destination):
            raise ValueError("Unsafe output path")
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_bytes(encoded)
        records.append({"source": relative.as_posix(), "output": output_relative.as_posix(),
                        "source_sha256": hashlib.sha256(raw).hexdigest(),
                        "sha256": hashlib.sha256(encoded).hexdigest(), "bytes": len(encoded),
                        **extra})
    return records


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--destination", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    args = parser.parse_args()
    records = stage(args.source, args.destination)
    report = {"source": str(args.source.resolve()), "destination": str(args.destination.resolve()),
              "format": "PCX v5, RLE, 8 bits, four RGBA planes", "files": records}
    args.manifest.parent.mkdir(parents=True, exist_ok=True)
    args.manifest.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"files": len(records), "manifest": str(args.manifest), "pixel_checks": "PASS"}))


if __name__ == "__main__":
    main()
