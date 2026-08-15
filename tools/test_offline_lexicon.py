#!/usr/bin/env python3
"""Deterministic validation and timing for CantoCandidate offline_lexicon.bin."""

from __future__ import annotations

import argparse
import bisect
import statistics
import struct
import time
from pathlib import Path

HEADER = struct.Struct("<8sII")
ENTRY_HEADER_V1 = struct.Struct("<HHH")
ENTRY_HEADER_V2 = struct.Struct("<HHHI")


def load_index(path: Path):
    data = path.read_bytes()
    magic, version, count = HEADER.unpack_from(data, 0)
    if magic != b"CANTOLEX" or version not in {1, 2}:
        raise ValueError("Invalid CANTOLEX index")
    entry_header = ENTRY_HEADER_V2 if version == 2 else ENTRY_HEADER_V1
    entries = []
    offset = HEADER.size
    for _ in range(count):
        fields = entry_header.unpack_from(data, offset)
        code_len, word_len, jp_len = fields[:3]
        offset += entry_header.size
        code = data[offset : offset + code_len].decode("utf-8")
        offset += code_len
        word = data[offset : offset + word_len].decode("utf-8")
        offset += word_len
        jyutping = data[offset : offset + jp_len].decode("utf-8")
        offset += jp_len
        entries.append((code, word, jyutping))
    if offset != len(data):
        raise ValueError("Unexpected trailing bytes")
    return entries


def normalize(value: str) -> str:
    return "".join(c for c in value.lower() if "a" <= c <= "z")


def lookup(entries, query: str, limit: int = 9):
    code = normalize(query)
    keys = [entry[0] for entry in entries]
    at = bisect.bisect_left(keys, code)
    result = []
    while at < len(entries) and entries[at][0] == code and len(result) < limit:
        if entries[at][1] not in result:
            result.append(entries[at][1])
        at += 1
    return result


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("index", type=Path)
    args = parser.parse_args()
    entries = load_index(args.index)
    queries = ["neihou", "m4goi", "hou2", "gwongdung", "hoenggong", "ngo", "sikfaan"]
    results = {query: lookup(entries, query) for query in queries}
    expected = {"neihou": "你好", "m4goi": "唔該", "gwongdung": "廣東", "hoenggong": "香港", "ngo": "我", "sikfaan": "食飯"}
    failures = [f"{query} missing {word}" for query, word in expected.items() if word not in results[query]]
    samples = ["neihou", "m4goi", "ngo", "heunggong", "gwongdung"] * 20
    elapsed = []
    for query in samples:
        started = time.perf_counter_ns()
        lookup(entries, query, 45)
        elapsed.append((time.perf_counter_ns() - started) / 1_000_000)
    print(f"entries={len(entries)}")
    print(f"average_ms={statistics.mean(elapsed):.4f}")
    print(f"p95_ms={sorted(elapsed)[int(len(elapsed) * 0.95) - 1]:.4f}")
    for query, words in results.items():
        print(f"{query}\t{' | '.join(words) if words else '(none)'}")
    if failures:
        raise SystemExit("; ".join(failures))


if __name__ == "__main__":
    main()
