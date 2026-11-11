#!/usr/bin/env python3

import json
import sys
from pathlib import Path

import msgpack


DATASETS = ["canada", "citm_catalog", "twitter"]


def main() -> None:
    project_dir = Path(__file__).resolve().parent.parent
    json_dir    = Path(sys.argv[1]) if len(sys.argv) > 1 else project_dir.parent / "json-benchmark" / "data"
    output_dir  = Path(sys.argv[2]) if len(sys.argv) > 2 else project_dir / "data"

    for name in DATASETS:
        source = json_dir / f"{name}.json"
        target = output_dir / f"{name}.msgpack"
        with open(source, encoding="utf-8") as f:
            document = json.load(f)
        packed = msgpack.packb(document, use_bin_type=True, use_single_float=False, strict_types=True)
        if msgpack.unpackb(packed, raw=False, strict_map_key=False) != document:
            sys.exit(f"{source}: round trip mismatch")
        target.write_bytes(packed)
        print(f"  {source.name} ({source.stat().st_size} bytes) -> {target.name} ({len(packed)} bytes)")


if __name__ == "__main__":
    main()
