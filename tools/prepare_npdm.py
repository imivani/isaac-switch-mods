"""Derive the injection descriptor from this user's verified original.

Only four SVC permissions and the lengths needed for their descriptors change.
No game code, filesystem/service permissions, title identity or thread settings
are replaced with a framework example. This is an Atmosphere development file.
"""
import argparse
import hashlib
import json
import pathlib
import struct

EXPECTED_SHA256 = "d325e6e0fdec64053c0efd7e2bf1cd679215b10772c410a91247e1d41bae4c5e"
ADDED_SVCS = {0x40, 0x43, 0x74, 0x75}


def u32(data, offset):
    return struct.unpack_from("<I", data, offset)[0]


def capabilities(data, acid):
    start = u32(data, 0x78 if acid else 0x70)
    relative, length = struct.unpack_from("<II", data, start + (0x230 if acid else 0x30))
    return list(struct.unpack_from("<" + "I" * (length // 4), data, start + relative))


def syscalls(words):
    result = set()
    for word in words:
        if word & 0x1F == 0x0F:
            group = word >> 29
            bits = (word >> 5) & 0xFFFFFF
            result.update(group * 24 + bit for bit in range(24) if bits & (1 << bit))
    return result


def derive(original):
    if hashlib.sha256(original).hexdigest() != EXPECTED_SHA256:
        raise ValueError("This is not the verified Isaac 1.7.9b main.npdm; refusing to patch")
    assert len(original) == 0x624 and original[:4] == b"META"
    assert original[0x454:0x460] == bytes(12)
    patched = bytearray(original) + bytearray(4)
    edits = {0x43C: 0x41200F8F, 0x60C: 0x41200F8F,
             0x454: 0x8600000F, 0x624: 0x8600000F,
             0x2B4: 40, 0x7C: 984, 0x284: 0x2D8,
             0x494: 40, 0x74: 456}
    # ACID has KAC offset at +0x230, size at +0x234, i.e. 0x2b4.
    for offset, value in edits.items():
        struct.pack_into("<I", patched, offset, value)
    for acid in (False, True):
        before, after = capabilities(original, acid), capabilities(patched, acid)
        assert syscalls(after) - syscalls(before) == ADDED_SVCS
        assert not syscalls(before) - syscalls(after)
        assert [w for w in before if w & 0x1F != 0x0F] == [w for w in after if w & 0x1F != 0x0F]
    allowed = {byte for offset in edits for byte in range(offset, offset + 4)}
    assert all(i in allowed for i, (a, b) in enumerate(zip(original, patched)) if a != b)
    return bytes(patched), edits


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("original", type=pathlib.Path)
    parser.add_argument("output", type=pathlib.Path)
    parser.add_argument("--report", type=pathlib.Path)
    args = parser.parse_args()
    if args.original.resolve() == args.output.resolve():
        raise ValueError("Original descriptor must remain untouched")
    original = args.original.read_bytes()
    result, edits = derive(original)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(result)
    report = {"source_sha256": hashlib.sha256(original).hexdigest(),
              "output_sha256": hashlib.sha256(result).hexdigest(),
              "source_bytes": len(original), "output_bytes": len(result),
              "added_syscalls": [hex(x) for x in sorted(ADDED_SVCS)],
              "word_edits": {hex(k): hex(v) for k, v in edits.items()},
              "preserved": "Original title, services, filesystem and thread settings",
              "validation": "Both capability lists decoded and exact four-permission delta verified"}
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
