#!/usr/bin/env python3
"""Validate Nitemare 3-D containers and lossless no-edit round trips."""
from __future__ import annotations

import argparse
import struct
from pathlib import Path

MAP_HEADER = 514
MAP_BYTES = 8192


def parse(path: Path):
    data = path.read_bytes()
    if len(data) >= MAP_HEADER + MAP_BYTES:
        count = struct.unpack_from("<H", data)[0]
        if count and len(data) == MAP_HEADER + count * MAP_BYTES:
            return "MAP", data[:MAP_HEADER], [data[MAP_HEADER+i*MAP_BYTES:MAP_HEADER+(i+1)*MAP_BYTES] for i in range(count)]
    if len(data) >= 18:
        first = struct.unpack_from("<I", data, 4)[0]
        if 8 <= first < len(data) and first % 4 == 0:
            entries, pos = [], first
            while pos < len(data):
                if len(data) - pos < 10:
                    break
                width, height = data[pos], data[pos + 1]
                size = 10 + width * height
                if not width or not height or pos + size > len(data):
                    break
                entries.append(data[pos:pos+size])
                pos += size
            if pos == len(data) and len(entries) > 1:
                return "IMG", data[:first], entries
    if len(data) >= 12:
        first = struct.unpack_from("<I", data, 2)[0]
        if 6 <= first <= len(data):
            entries = []
            for pos in range(0, first - 5, 6):
                size, offset = struct.unpack_from("<HI", data, pos)
                if size == 0 and offset == 0:
                    entries.append(b"")
                    continue
                if offset < first or offset + size > len(data):
                    break
                entries.append(data[offset:offset+size])
                if offset + size == len(data):
                    return "DAT", data[:first], entries
    raise ValueError(f"unsupported or corrupt Nitemare file: {path}")


def rebuild(kind: str, header: bytes, entries: list[bytes]) -> bytes:
    if kind == "IMG":
        return header + b"".join(entries)
    if kind == "MAP":
        output = bytearray(header)
        struct.pack_into("<H", output, 0, len(entries))
        return bytes(output) + b"".join(entries)
    # Same-size DAT round trip keeps original offsets, including shared and
    # out-of-order sound blocks used by SND.DAT.
    total = max((struct.unpack_from("<I", header, i * 6 + 2)[0] + len(entry)
                 for i, entry in enumerate(entries) if entry), default=len(header))
    directory = bytearray(header) + bytearray(total - len(header))
    for index, entry in enumerate(entries):
        if entry:
            offset = struct.unpack_from("<I", header, index * 6 + 2)[0]
            directory[offset:offset+len(entry)] = entry
    return bytes(directory)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("files", nargs="+", type=Path)
    args = parser.parse_args()
    for path in args.files:
        kind, header, entries = parse(path)
        rebuilt = rebuild(kind, header, entries)
        if rebuilt != path.read_bytes():
            raise RuntimeError(f"round trip differs: {path}")
        print(f"OK  {path.name:10} {kind} entries={len(entries):4} bytes={len(rebuilt)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
