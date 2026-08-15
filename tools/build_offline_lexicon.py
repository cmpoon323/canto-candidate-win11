#!/usr/bin/env python3
"""Build CantoCandidate's deterministic offline Jyutping lexicon.

The compiler consumes Rime-Cantonese data locally. It preserves direct word+
Jyutping mappings and derives extra high-frequency essay entries only where every
character has exactly one known Jyutping reading. This avoids guessing ambiguous
multi-pronunciation phrases while covering common items such as 你好.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
from collections import defaultdict
from pathlib import Path

MAGIC = b"CANTOLEX"
VERSION = 2
ENTRY_HEADER = struct.Struct("<HHHI")
FILE_HEADER = struct.Struct("<8sII")
DIRECT_RE = re.compile(r"^(\S+)\s+([a-z]+(?:[1-6])?(?:\s+[a-z]+(?:[1-6])?)*)")
FREQUENCY_RE = re.compile(r"^(\S+)\s+(\d+)$")
MAX_DERIVED_CHARS = 12
MIN_DERIVED_FREQUENCY = 1000
MAX_DERIVED_CODES_PER_WORD = 4


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def normalize_jyutping(jyutping: str) -> str:
    """Canonical query key: lower case with tones, spaces and apostrophes removed."""
    return "".join(ch for ch in jyutping.lower() if "a" <= ch <= "z")


def parse_direct(path: Path):
    with path.open("r", encoding="utf-8") as handle:
        for source_line in handle:
            line = source_line.strip()
            if not line or line.startswith("#") or line in {"---", "..."} or ":" in line:
                continue
            match = DIRECT_RE.match(line)
            if not match:
                continue
            word, jyutping = match.groups()
            normalized = normalize_jyutping(jyutping)
            if word and normalized:
                yield word, jyutping, normalized


def parse_frequency(path: Path):
    frequencies: dict[str, int] = {}
    with path.open("r", encoding="utf-8") as handle:
        for source_line in handle:
            match = FREQUENCY_RE.match(source_line.strip())
            if match:
                word, count = match.groups()
                frequencies[word] = int(count)
    return frequencies


def build(source_dir: Path, output: Path, report: Path) -> None:
    chars_path = source_dir / "jyut6ping3.chars.dict.yaml"
    words_path = source_dir / "jyut6ping3.words.dict.yaml"
    essay_path = source_dir / "essay-cantonese.txt"
    source_files = [chars_path, words_path, essay_path]
    for path in source_files:
        if not path.is_file():
            raise FileNotFoundError(f"Required source data missing: {path}")

    source_hashes = {path.name: sha256_file(path) for path in source_files}
    frequencies = parse_frequency(essay_path)
    readings_by_char: dict[str, set[str]] = defaultdict(set)
    direct = []
    for word, jyutping, normalized in parse_direct(chars_path):
        direct.append((word, jyutping, normalized))
        if len(word) == 1:
            readings_by_char[word].add(jyutping)
    for word, jyutping, normalized in parse_direct(words_path):
        direct.append((word, jyutping, normalized))

    # Key is (normalized input, displayed word, displayed Jyutping). Higher
    # frequency wins for duplicated data from different source files.
    records: dict[tuple[str, str, str], int] = {}
    for word, jyutping, normalized in direct:
        records[(normalized, word, jyutping)] = max(records.get((normalized, word, jyutping), 0), frequencies.get(word, 0))

    derived_count = 0
    for word, frequency in frequencies.items():
        if len(word) < 2 or len(word) > MAX_DERIVED_CHARS or frequency < MIN_DERIVED_FREQUENCY:
            continue
        normalized_variants = {""}
        display_readings = []
        for character in word:
            readings = sorted(readings_by_char.get(character, set()))
            normalized_readings = sorted({normalize_jyutping(reading) for reading in readings if normalize_jyutping(reading)})
            if not normalized_readings:
                normalized_variants = set()
                break
            # Keep a bounded set of possible tone-insensitive spellings. The
            # displayed reading is explanatory only; matching uses the code.
            display_readings.append(readings[0])
            normalized_variants = {
                prefix + suffix
                for prefix in normalized_variants
                for suffix in normalized_readings
            }
            if len(normalized_variants) > MAX_DERIVED_CODES_PER_WORD:
                normalized_variants = set(sorted(normalized_variants)[:MAX_DERIVED_CODES_PER_WORD])
        if not normalized_variants:
            continue
        jyutping = " ".join(display_readings)
        for normalized in normalized_variants:
            key = (normalized, word, jyutping)
            if key not in records:
                records[key] = frequency
                derived_count += 1

    ordered = sorted(records.items(), key=lambda item: (item[0][0], -item[1], item[0][1], item[0][2]))
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("wb") as handle:
        handle.write(FILE_HEADER.pack(MAGIC, VERSION, len(ordered)))
        for (normalized, word, jyutping), weight in ordered:
            code = normalized.encode("utf-8")
            text = word.encode("utf-8")
            display = jyutping.encode("utf-8")
            if max(len(code), len(text), len(display)) > 65535:
                raise ValueError(f"Entry exceeds binary length limit: {word}")
            handle.write(ENTRY_HEADER.pack(len(code), len(text), len(display), min(weight, 0xFFFFFFFF)))
            handle.write(code)
            handle.write(text)
            handle.write(display)

    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(
        json.dumps(
            {
                "format": "CANTOLEX",
                "format_version": VERSION,
                "source_repository": "https://github.com/rime/rime-cantonese",
                "included_source_files": list(source_hashes),
                "source_sha256": source_hashes,
                "direct_entries": len(direct),
                "derived_frequency_phrase_codes": derived_count,
                "unique_entries": len(ordered),
                "index_bytes": output.stat().st_size,
                "index_sha256": sha256_file(output),
                "excluded_data": [
                    "jyut6ping3.phrase.dict.yaml (unannotated phrases)",
                    "jyut6ping3.maps.dict.yaml (ODbL data not used in v0.9 MVP)",
                ],
            },
            ensure_ascii=False,
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )


def main() -> None:
    parser = argparse.ArgumentParser(description="Build the CantoCandidate offline lexicon")
    parser.add_argument("--source", type=Path, required=True, help="Rime-Cantonese source directory")
    parser.add_argument("--output", type=Path, required=True, help="Output offline_lexicon.bin")
    parser.add_argument("--report", type=Path, required=True, help="Output build JSON report")
    args = parser.parse_args()
    build(args.source, args.output, args.report)


if __name__ == "__main__":
    main()
