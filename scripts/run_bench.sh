#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${1:-$PROJECT_DIR/build}"
RESULTS_DIR="$PROJECT_DIR/results"
PLOTS_DIR="$PROJECT_DIR/plots"
BENCH_CPU="${BENCH_CPU:-2}"

cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
rm -f "$BUILD_DIR"/benchmarks/performance/bench_*
cmake --build "$BUILD_DIR" --parallel $(( $(nproc) > 1 ? $(nproc) - 1 : 1 ))

governor=$(cat /sys/devices/system/cpu/cpu"$BENCH_CPU"/cpufreq/scaling_governor 2>/dev/null || echo unknown)
boost=$(cat /sys/devices/system/cpu/cpufreq/boost 2>/dev/null || echo unknown)
if [ "$governor" != performance ] || [ "$boost" != 0 ]; then
    echo "governor=$governor boost=$boost, expected performance and 0:" >&2
    echo "  echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor" >&2
    echo "  echo 0 | sudo tee /sys/devices/system/cpu/cpufreq/boost" >&2
    [ "${ALLOW_NOISY:-0}" = 1 ] || exit 1
fi

cpufreq=/sys/devices/system/cpu/cpu"$BENCH_CPU"/cpufreq
target=$(( $(cat "$cpufreq"/scaling_max_freq) * 98 / 100000 ))
echo ""
echo "=== Waiting for the CPU to settle (cpu$BENCH_CPU >= $target MHz) ==="
for _ in $(seq 120); do
    mhz=$(( $(cat "$cpufreq"/scaling_cur_freq) / 1000 ))
    [ "$mhz" -ge "$target" ] && break
    sleep 5
done
echo "  starting at $mhz MHz"
[ "$mhz" -ge "$target" ] || [ "${ALLOW_NOISY:-0}" = 1 ] || exit 1

mkdir -p "$RESULTS_DIR"

echo ""
echo "=== Conformance ==="
"$BUILD_DIR/benchmarks/conformance/conformance" --output "$RESULTS_DIR/conformance.json"

REPETITIONS="${REPETITIONS:-3}"

export GLIBC_TUNABLES="glibc.malloc.mmap_threshold=33554432:glibc.malloc.trim_threshold=4294967296"

rm -rf "$RESULTS_DIR/runs"
mkdir -p "$RESULTS_DIR/runs"

echo ""
echo "=== Running benchmarks ($REPETITIONS repetitions) ==="
for rep in $(seq "$REPETITIONS"); do
    for bench in "$BUILD_DIR"/benchmarks/performance/bench_*; do
        [ -x "$bench" ] || continue
        name="$(basename "$bench")"
        name="${name#bench_}"
        echo "  repetition $rep/$REPETITIONS: $name"
        taskset -c "$BENCH_CPU" "$bench" \
            --benchmark_format=json \
            --benchmark_out="$RESULTS_DIR/runs/${name}_$rep.json" \
            --benchmark_min_warmup_time=0.5 > /dev/null
    done
done

echo ""
echo "=== Generating plots ==="
python3 "$PLOTS_DIR/plot.py" "$RESULTS_DIR/conformance.json" "$RESULTS_DIR"/runs/*.json
