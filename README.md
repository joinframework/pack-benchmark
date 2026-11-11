# pack-benchmark

Comparative benchmarks of MessagePack libraries in C++, measuring parse, serialize, and DOM statistics performance across three real-world datasets (Canada, CitmCatalog, Twitter). Includes a conformance suite covering format acceptance, rejection of malformed input, and value-preserving roundtrip.

Companion of [json-benchmark](https://github.com/joinframework/json-benchmark), with the same protocol and layout.

## Table of contents

- [Libraries](#libraries)
- [Datasets](#datasets)
- [Build](#build)
- [Running benchmarks](#running-benchmarks)
- [Plots](#plots)
- [What the benchmarks measure](#what-the-benchmarks-measure)
  - [Parse](#parse)
  - [Serialize](#serialize)
  - [Statistics](#statistics)
  - [Conformance](#conformance)
- [Notes](#notes)
- [License](#license)

## Libraries

| Library | Parse | Serialize | Statistics | Conformance |
|---------|-------|-----------|------------|-------------|
| [Join](https://github.com/joinframework/join) | ✓ | ✓ | ✓ | ✓ |
| [jsoncons](https://github.com/danielaparker/jsoncons) | ✓ | ✓ | ✓ | ✓ |
| [MPack](https://github.com/ludocode/mpack) (zero-copy) | ✓ | ✓ | ✓ | ✓ |
| [msgpack-cxx](https://github.com/msgpack/msgpack-c/tree/cpp_master) (default) | ✓ | ✓ | ✓ | ✓ |
| [msgpack-cxx](https://github.com/msgpack/msgpack-c/tree/cpp_master) (zero-copy) | ✓ | ✓ | ✓ | ✓ |
| [nlohmann/json](https://github.com/nlohmann/json) | ✓ | ✓ | ✓ | ✓ |
| [taocpp/json](https://github.com/taocpp/json) | ✓ | ✓ | ✓ | ✓ |

All dependencies are fetched automatically via CMake FetchContent, at pinned versions (`cmake/deps.cmake`).

Libraries fall into two categories, marked in the plots by hatched bars for the zero-copy ones:
- **copying** — the DOM owns its strings, binaries and extension payloads, and the input buffer can be released after parse;
- **zero-copy** — the DOM references the input buffer for strings, binaries and extension payloads, which must outlive the DOM. Parse then does no string copy, and statistics or serialize read the payloads from the input. Their figures are not like-for-like with the copying libraries.

**msgpack-cxx** is used header-only, without Boost (`MSGPACK_NO_BOOST`). Its DOM is a `msgpack::object` tree allocated in a `msgpack::zone`. It is benchmarked in two configurations:
- **default** — no `unpack_reference_func`: strings, binaries and extensions are copied into the zone;
- **zero-copy** — an `unpack_reference_func` returning true for `STR`, `BIN` and `EXT`, so their payloads reference the input buffer.

**MPack** is a C library, built from its sources with `MPACK_EXTENSIONS=1`: without it, extension types are rejected as unsupported. Its node tree (`mpack_tree_t`) is read-only and always references the input buffer, so it is zero-copy only. It is serialized by walking the tree with the `mpack_writer_t` API into a growable buffer.

**nlohmann/json**, **jsoncons** and **taocpp/json** are JSON libraries with a MessagePack codec on top of their JSON DOM (`from_msgpack`/`to_msgpack`, `decode_msgpack`/`encode_msgpack`, `msgpack::from_string`/`msgpack::to_string`). Their object keys are strings: nlohmann/json and taocpp/json reject maps with other keys, jsoncons converts such keys to their text form. taocpp/json rejects extension types. jsoncons (`jsoncons::json`), nlohmann/json and taocpp/json store objects sorted by key, so their output does not keep the input key order.

**Join** stores `bin` and extension types as binary values, an extension keeping its type as the binary subtype (timestamps are extensions of type -1, not converted); map keys may be nil, booleans, numbers, strings, `bin` or extensions.

Every adapter decodes a whole document: input left over after the first object is rejected. Join, nlohmann/json (`from_msgpack` with `strict = true`) and taocpp/json (whose grammar ends with `eof`) check it natively. msgpack-cxx, MPack and jsoncons decode one object from a stream and report the bytes they used (`unpack` offset, `mpack_tree_size`, `msgpack_bytes_reader::column`); their adapters reject the input when that count is not its size. jsoncons is therefore decoded with `msgpack_bytes_reader` and a `json_decoder`, since `decode_msgpack` ignores trailing bytes.

Output buffers follow one rule: each library writes into its native output buffer (`std::string` for jsoncons, nlohmann/json and taocpp/json, `msgpack::sbuffer` for msgpack-cxx, MPack's growable writer). When the adapter has to provide the buffer, as for Join's `std::ostream`, it starts at 8 KiB and doubles when full.

## Datasets

| File | Size | Characteristics |
|------|------|-----------------|
| `canada.msgpack` | ~1.1 MB | Geometry data — float-heavy (coordinate pairs), large arrays |
| `citm_catalog.msgpack` | ~0.3 MB | Event catalog — deeply nested objects, mixed types |
| `twitter.msgpack` | ~0.4 MB | Social feed — Unicode strings, many small objects |

The files are the json-benchmark datasets converted with the reference Python implementation ([msgpack](https://pypi.org/project/msgpack/)), using the minimal encoding: smallest integer format, every JSON floating-point number as `float64`, strings as `str`. To regenerate them:

```bash
python3 -m venv .venv
.venv/bin/pip install msgpack
.venv/bin/python -I scripts/gen_data.py ../json-benchmark/data data
```

Each benchmark binary is run several times, each repetition in a new process. The time reported is the minimum across repetitions, and the overall time is the sum over the three datasets (ms), computed per repetition before taking the minimum.

## Build

Configure:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
```

Join is fetched from its `main` branch; to build against a local working tree instead:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DFETCHCONTENT_SOURCE_DIR_JOIN=/path/to/join
```

Compile:

```bash
cmake --build build -j$(nproc)
```

## Running benchmarks

Run everything and generate plots:

```bash
bash scripts/run_bench.sh
```

Single binary (one per operation and library, `bench_<operation>_<library>`):

```bash
GLIBC_TUNABLES=glibc.malloc.mmap_threshold=33554432:glibc.malloc.trim_threshold=4294967296 \
taskset -c 2 ./build/benchmarks/performance/bench_parse_mpack \
    --benchmark_format=json \
    --benchmark_out=results/parse_mpack.json \
    --benchmark_min_warmup_time=0.5
```

Conformance only:

```bash
./build/benchmarks/conformance/conformance --output results/conformance.json
```

> [!NOTE]
> Default: 3 repetitions per binary (`REPETITIONS`), each in a new process, minimum reported. Override with e.g. `REPETITIONS=5 bash scripts/run_bench.sh`. Raw per-repetition results are written to `results/runs/`.

## Plots

Install plotting dependencies, either from the distribution (Debian/Ubuntu):

```bash
sudo apt install python3-matplotlib python3-pandas
```

or in a virtual environment:

```bash
python3 -m venv .venv
.venv/bin/pip install -r plots/requirements.txt
```

Generate the charts:

```bash
python3 plots/plot.py results/conformance.json results/runs/*.json
```

Bars show the minimum across processes.

PNGs are written to `plots/`:
- `performance_parse.png` — parse (overall + per dataset) and DOM statistics
- `performance_serialize.png` — serialize (overall + per dataset)
- `conformance.png` — overall score followed by one subplot per score (accept and roundtrip for core, binary & ext and non-string keys, then reject)

## What the benchmarks measure

### Parse

**Goal**: time to decode a MessagePack document into an in-memory DOM.

The benchmark calls `Adapter::parse(msgpack)` and measures only the decoding time. DOM destruction is excluded from timing (`PauseTiming` / `ResumeTiming`), as in json-benchmark.

**What is measured**: format decoding, byte-order conversion, and DOM construction.

**What is not measured**: DOM traversal, serialization, input I/O.

---

### Serialize

**Goal**: time to serialize a pre-decoded DOM back to MessagePack bytes.

The document is decoded once before the timing loop; each iteration serializes the same DOM into the library's native output buffer.

**What is measured**: DOM traversal and MessagePack generation.

Each library picks its own encoding. Join, jsoncons and nlohmann/json write a `float64` as `float32` when the conversion is exact, so their output on `canada.msgpack` is 648 bytes shorter (1 056 145 instead of 1 056 793); the others keep `float64`. MPack and msgpack-cxx reproduce the three inputs byte for byte; Join does on `citm_catalog.msgpack` and `twitter.msgpack`; beyond the `float32` narrowing, jsoncons, nlohmann/json and taocpp/json differ from the input only by their sorted key order.

---

### Statistics

**Goal**: time to traverse the entire DOM and accumulate counts of each node type (maps, arrays, strings, numbers, booleans, nils).

The document is decoded once before the timing loop. Each iteration walks every node in the tree. A map key is counted as a value of its own type (a string key as a string, an integer key as a number), so the counts stay comparable with json-benchmark's, where every key is a string.

---

### Conformance

Run once (no timing), on hand-written MessagePack byte strings. The 80 valid documents are split into three categories, each scored twice — **accept** (the library decodes it) and **roundtrip** (decode, serialize, and compare the output value by value with the input):

1. **Core** (55 cases): nil and booleans, positive and negative fixint, `uint8/16/32/64` and `int8/16/32/64` at their bounds, non-minimal integer, string and array encodings, `float32` and `float64` (including `-0.0`, subnormals, infinities and NaN), `fixstr/str8/str16/str32` (including an embedded NUL and UTF-8), empty and nested arrays and maps, `array16/32` and `map16/32` with string keys, and 100 nested arrays.
2. **Binary & Ext** (16 cases): `bin8/16/32`, `fixext1..16`, `ext8/16/32`, and the three timestamp formats.
3. **Non-string Keys** (9 cases): maps keyed by a positive, negative or `uint64` integer, nil, booleans, a float, a `bin`, an array, or a map.

**Reject** (35 cases): empty input, the never-used byte `0xc1`, every fixed-size format truncated, string, binary and extension payloads shorter than their header, arrays and maps missing elements, `str32`, `bin32`, `array32` and `map32` headers declaring 4 GiB with no data, and a trailing byte after a complete document.

The roundtrip is judged by a small MessagePack decoder in `conformance/main.cpp`, independent of every library under test. It decodes both documents into a plain value tree and compares them: integers by value whatever their encoding width or signedness, floating-point numbers bitwise as `double` after widening `float32` (`float32` 0.1 and the `float64` holding the same value are equal), strings and binaries byte for byte and never with each other, extensions by type and payload, maps key by key regardless of order.

In addition, an **output check** guards the performance benchmarks: on each dataset, the DOM statistics of every library must match, re-decoding a library's own serialize output must yield the same statistics, and the output must equal the input value by value (same comparison as the roundtrip). Any mismatch is reported on stderr and makes `conformance` exit with status 1, which stops `run_bench.sh` before any timing.

## Notes

- `-march=native`: CPU-specific SIMD optimisations. Results are not portable across machines.
- Every binary is compiled as C++20 (`CMAKE_CXX_STANDARD 20`), the standard Join requires.
- Each library is measured in its own binary (`bench_<operation>_<library>`), linked only against that library, and each repetition runs in a new process, interleaved across libraries (see json-benchmark for the measurements behind both choices).
- The allocator policy is fixed for every library (`GLIBC_TUNABLES`: 32 MB `mmap` threshold, trimming disabled), and `--benchmark_min_warmup_time=0.5` lets the heap reach its steady-state size before measurement. Each benchmark reports its page faults per iteration as the `minflt` counter; `plot.py` warns when it is not zero.
- Benchmarks are pinned with `taskset -c $BENCH_CPU` (default 2). They are single-threaded, so pinning the process pins the measured thread.
- Build and measurement are separated. `run_bench.sh` refuses to measure unless the governor is `performance` and boost is disabled, then waits until the current frequency of `$BENCH_CPU` (`scaling_cur_freq`) is back to 98% of its maximum (`scaling_max_freq`). It reads `cpufreq` in sysfs rather than `/proc/cpuinfo`, which has no `cpu MHz` line on ARM. `ALLOW_NOISY=1` overrides both checks.
- Every compiled library is linked statically, and every library runs with its default allocator.
- Serialize returns each library's output buffer (see the buffer rule under [Libraries](#libraries)) without converting it.
- Result JSONs are gitignored (`results/`).

## License

Licensed under the [MIT License](LICENSE).
