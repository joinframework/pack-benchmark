#!/usr/bin/env python3

import json
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd
from matplotlib.colors import to_rgb


LIBRARY_LABELS = {
    "JoinAdapter":          "join",
    "JsonConsAdapter":      "jsoncons",
    "MpackZeroCopyAdapter": "mpack (zero-copy)",
    "MsgpackCxxAdapter":    "msgpack-cxx",
    "MsgpackCxxRefAdapter": "msgpack-cxx (zero-copy)",
    "NlohmannAdapter":      "nlohmann",
    "TaoCppAdapter":        "taocpp",
}

LIBRARY_ORDER = list(LIBRARY_LABELS.values())

DATASET_ORDER = ["Canada", "Citm", "Twitter"]

_FAMILY_COLORS = {
    "join":        "#2ca02c",
    "jsoncons":    "#d62728",
    "mpack":       "#9467bd",
    "msgpack-cxx": "#1f77b4",
    "nlohmann":    "#8c564b",
    "taocpp":      "#e6ab02",
}
_LIB_COLORS: dict[str, tuple] = {}

ZERO_COPY_HATCH = "///"

def _family(name: str) -> str:
    return name.split(" (")[0].split("/")[0]

def _set_hatches(bars, libs: list[str]) -> None:
    for bar, lib in zip(bars, libs):
        if "(zero-copy)" in lib:
            bar.set_hatch(ZERO_COPY_HATCH)

def _lib_color(name: str) -> tuple:
    if name not in _LIB_COLORS:
        family   = _family(name)
        variants = sorted((lib for lib in LIBRARY_ORDER if _family(lib) == family), key=lambda lib: lib != family)
        rank     = variants.index(name) if name in variants else 0
        r, g, b  = to_rgb(_FAMILY_COLORS.get(family, "#7f7f7f"))
        t        = 0.55 * rank / max(len(variants), 1)
        _LIB_COLORS[name] = (r + (1 - r) * t, g + (1 - g) * t, b + (1 - b) * t)
    return _LIB_COLORS[name]

def load_results(paths: list[str]) -> pd.DataFrame:
    rows = []
    for path in paths:
        with open(path) as f:
            data = json.load(f)
        for b in data.get("benchmarks", []):
            if b.get("run_type") != "iteration" or b.get("error_occurred"):
                continue
            if b.get("minflt", 0) > 0.5:
                print(f"  warning: {b['name']} ({path}): {b['minflt']:.1f} page faults per iteration", file=sys.stderr)
            parts = b["name"].split("/")
            if len(parts) < 3:
                continue
            rows.append({
                "type":    parts[0],
                "dataset": parts[1],
                "library": LIBRARY_LABELS.get(parts[2], parts[2]),
                "run":     path,
                "time_ms": b.get("real_time", 0),
            })
    return pd.DataFrame(rows)

def _summarise(df: pd.DataFrame) -> pd.Series:
    return df.groupby(["library", "run"])["time_ms"].sum().groupby("library").min().sort_index(ascending=False)

def _style_ax(ax: plt.Axes) -> None:
    ax.grid(axis="x", alpha=0.3, linestyle="--")
    ax.set_axisbelow(True)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)

def _barh_ax(ax: plt.Axes, summary: pd.Series, title: str) -> None:
    libs   = summary.index.tolist()
    values = summary.tolist()
    colors = [_lib_color(lib) for lib in libs]
    bars = ax.barh(libs, values, color=colors, alpha=0.87, height=0.65, edgecolor="white")
    _set_hatches(bars, libs)
    ax.bar_label(bars, labels=[f"{v:.3g}" for v in values], padding=4, fontsize=8)
    ax.set_title(title, fontsize=11, fontweight="bold", loc="left", pad=6)
    ax.set_xlabel("Time (ms)")
    ax.set_xlim(0, summary.max() * 1.18 if not summary.empty else 1)
    _style_ax(ax)

def plot_deserialize(df: pd.DataFrame, output_dir: Path) -> None:
    parse_df = df[df["type"] == "Parse"]
    stats_df = df[df["type"] == "Statistics"]
    if parse_df.empty:
        return

    datasets  = [d for d in DATASET_ORDER if d in parse_df["dataset"].unique()]
    has_stats = not stats_df.empty
    n_subs    = 1 + len(datasets) + (1 if has_stats else 0)
    n_libs    = parse_df["library"].nunique()
    row_h     = max(3.5, n_libs * 0.38 + 0.8)

    fig, axes = plt.subplots(n_subs, 1, figsize=(10, row_h * n_subs))
    if n_subs == 1:
        axes = [axes]
    fig.suptitle("MessagePack Benchmark — Parse", fontsize=14, fontweight="bold")

    idx = 0
    _barh_ax(axes[idx], _summarise(parse_df), "Overall")
    idx += 1

    for dataset in datasets:
        _barh_ax(axes[idx], _summarise(parse_df[parse_df["dataset"] == dataset]), dataset)
        idx += 1

    if has_stats:
        _barh_ax(axes[idx], _summarise(stats_df), "DOM Statistics")

    fig.tight_layout(rect=[0, 0, 1, 0.97])
    out = output_dir / "performance_parse.png"
    fig.savefig(out, dpi=150, bbox_inches="tight")
    print(f"  Saved: {out}")
    plt.close(fig)

def _plot_write_bench(df: pd.DataFrame, output_dir: Path, bench_type: str, filename: str, title: str) -> None:
    sub_df = df[df["type"] == bench_type]
    if sub_df.empty:
        return

    datasets  = [d for d in DATASET_ORDER if d in sub_df["dataset"].unique()]
    n_subs    = 1 + len(datasets)
    n_libs    = sub_df["library"].nunique()
    row_h     = max(3.5, n_libs * 0.38 + 0.8)

    fig, axes = plt.subplots(n_subs, 1, figsize=(10, row_h * n_subs))
    if n_subs == 1:
        axes = [axes]
    fig.suptitle(title, fontsize=14, fontweight="bold")

    _barh_ax(axes[0], _summarise(sub_df), "Overall")

    for idx, dataset in enumerate(datasets, start=1):
        _barh_ax(axes[idx], _summarise(sub_df[sub_df["dataset"] == dataset]), dataset)

    fig.tight_layout(rect=[0, 0, 1, 0.97])
    out = output_dir / filename
    fig.savefig(out, dpi=150, bbox_inches="tight")
    print(f"  Saved: {out}")
    plt.close(fig)

def plot_serialize(df: pd.DataFrame, output_dir: Path) -> None:
    _plot_write_bench(df, output_dir, "Serialize", "performance_serialize.png", "MessagePack Benchmark — Serialize")

_CONF_CATEGORIES = [
    "core_accept", "core_roundtrip",
    "binary_ext_accept", "binary_ext_roundtrip",
    "non_string_keys_accept", "non_string_keys_roundtrip",
    "reject",
]
_CONF_CAT_LABELS = [
    "Core — Accept", "Core — Roundtrip",
    "Binary & Ext — Accept", "Binary & Ext — Roundtrip",
    "Non-string Keys — Accept", "Non-string Keys — Roundtrip",
    "Reject Invalid",
]


def plot_conformance(conf_path: str, output_dir: Path) -> None:
    with open(conf_path) as f:
        data = json.load(f)
    results = data.get("results", [])
    if not results:
        return

    rows = []
    for r in results:
        lib = r["library"]
        total_all = sum(r[c]["total"] for c in _CONF_CATEGORIES if c in r)
        if total_all == 0:
            continue
        pass_all = sum(r[c]["pass"] for c in _CONF_CATEGORIES if c in r)
        row = {"library": lib, "overall": pass_all / total_all * 100}
        for c in _CONF_CATEGORIES:
            row[c] = r[c]["pass"] / r[c]["total"] * 100 if c in r and r[c]["total"] > 0 else 0.0
        rows.append(row)

    if not rows:
        return

    rows.sort(key=lambda r: r["library"], reverse=True)

    all_keys   = ["overall"] + _CONF_CATEGORIES
    all_labels = ["Overall"] + _CONF_CAT_LABELS

    n_libs = len(rows)
    row_h  = max(3.0, n_libs * 0.38 + 0.7)

    fig, axes = plt.subplots(len(all_keys), 1,
                             figsize=(10, row_h * len(all_keys)))
    fig.suptitle("MessagePack Benchmark — Conformance", fontsize=14, fontweight="bold")

    for ax, c, label in zip(axes, all_keys, all_labels):
        libs   = [r["library"] for r in rows]
        vals   = [r[c] for r in rows]
        colors = [_lib_color(lib) for lib in libs]

        bars = ax.barh(libs, vals, color=colors, alpha=0.87, height=0.65, edgecolor="white")
        _set_hatches(bars, libs)
        for i, v in enumerate(vals):
            ax.text(min(v + 0.8, 101), i, f"{v:.0f}%", va="center", fontsize=8)

        ax.set_xlabel("%")
        ax.set_xlim(0, 115)
        ax.set_title(label, fontsize=11, fontweight="bold", loc="left", pad=6)
        _style_ax(ax)

    fig.tight_layout(rect=[0, 0, 1, 0.97])
    out = output_dir / "conformance.png"
    fig.savefig(out, dpi=150, bbox_inches="tight")
    print(f"  Saved: {out}")
    plt.close(fig)

def _is_conformance(path: str) -> bool:
    try:
        with open(path) as f:
            data = json.load(f)
        return "results" in data and "benchmarks" not in data
    except Exception:
        return False


def main() -> None:
    if len(sys.argv) < 2:
        sys.exit(1)

    output_dir = Path(__file__).parent
    print(f"Generating plots in: {output_dir}")

    bench_paths = [p for p in sys.argv[1:] if not _is_conformance(p)]
    conf_paths  = [p for p in sys.argv[1:] if _is_conformance(p)]

    if bench_paths:
        df = load_results(bench_paths)
        if not df.empty:
            plot_deserialize(df, output_dir)
            plot_serialize(df, output_dir)

    for conf_path in conf_paths:
        plot_conformance(conf_path, output_dir)

    print("Done.")


if __name__ == "__main__":
    main()
