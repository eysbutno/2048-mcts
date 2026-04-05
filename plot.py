"""
NOTE: This part is completely vibe coded, because I am much too lazy to 
write matplotlib code. But it should work.

2048 MCTS Benchmarking Script
==============================
Runs the compiled MCTS solver at different rollout counts, collects results,
and plots cumulative score distributions + summary stats.
 
PREREQUISITE
------------
Your main.cpp (or a dedicated bench binary) must accept rollout count as argv[1]:
 
    int rollouts = argc > 1 ? std::atoi(argv[1]) : 1000;
    solver.search_rollouts(rollouts);
 
Then compile and set EXECUTABLE below.
 
Expected stdout per run (current format):
    <moves_per_sec>
    <total_seconds> <final_score>
    <row0: 4 ints>
    <row1: 4 ints>
    <row2: 4 ints>
    <row3: 4 ints>
"""
 
import subprocess
import time
import sys
import threading
from pathlib import Path
from collections import defaultdict
from concurrent.futures import ThreadPoolExecutor, as_completed
 
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.ticker as mticker
from matplotlib.gridspec import GridSpec
 
# ── Configuration ─────────────────────────────────────────────────────────────
 
EXECUTABLE     = "./run.exe"        # path to compiled binary
ROLLOUT_COUNTS = [500, 1000, 5000]  # rollouts to benchmark
RUNS_PER_LEVEL = 10                 # games per rollout count
TIMEOUT_SEC    = 1000               # kill a run after this many seconds
MAX_WORKERS    = 12                 # parallel games to run at once
 
# ── Parsing ────────────────────────────────────────────────────────────────────
 
def parse_output(stdout: str) -> dict | None:
    """Parse one game's stdout into a result dict. Returns None on parse error."""
    lines = [l.strip() for l in stdout.strip().splitlines() if l.strip()]
    if len(lines) < 6:
        return None
    try:
        moves_per_sec = float(lines[0])
        parts = lines[1].split()
        seconds = int(parts[0])
        score   = int(parts[1])
        board   = []
        for row_line in lines[2:6]:
            board.append([int(x) for x in row_line.split()])
        max_tile = max(v for row in board for v in row)
        return {
            "moves_per_sec": moves_per_sec,
            "seconds":       seconds,
            "score":         score,
            "max_tile":      max_tile,
            "board":         board,
        }
    except (ValueError, IndexError):
        return None
 
# ── Runner ─────────────────────────────────────────────────────────────────────

_print_lock = threading.Lock()

def run_game(rollouts: int, run_idx: int, total: int) -> dict | None:
    label = f"rollouts={rollouts} run {run_idx+1}/{total}"
    with _print_lock:
        print(f"  {label} ...", end=" ", flush=True)
    t0 = time.perf_counter()
    try:
        proc = subprocess.run(
            [EXECUTABLE, str(rollouts)],
            capture_output=True, text=True,
            timeout=TIMEOUT_SEC,
        )
        elapsed = time.perf_counter() - t0
        result = parse_output(proc.stdout)
        with _print_lock:
            if result:
                print(f"score={result['score']:,}  max_tile={result['max_tile']}  ({elapsed:.1f}s)")
            else:
                print(f"PARSE ERROR  stderr: {proc.stderr[:80]}")
        return result
    except subprocess.TimeoutExpired:
        with _print_lock:
            print(f"TIMEOUT after {TIMEOUT_SEC}s")
        return None
    except FileNotFoundError:
        with _print_lock:
            print(f"\nERROR: executable '{EXECUTABLE}' not found.")
        sys.exit(1)
 
def run_benchmark() -> dict[int, list[dict]]:
    results: dict[int, list[dict]] = defaultdict(list)
    total = len(ROLLOUT_COUNTS) * RUNS_PER_LEVEL
    done  = 0
    done_lock = threading.Lock()

    for rollouts in ROLLOUT_COUNTS:
        print(f"\n── Rollouts = {rollouts} (workers={MAX_WORKERS}) ──────────────────────────")

        futures_map = {}
        with ThreadPoolExecutor(max_workers=MAX_WORKERS) as executor:
            for i in range(RUNS_PER_LEVEL):
                future = executor.submit(run_game, rollouts, i, RUNS_PER_LEVEL)
                futures_map[future] = i

            for future in as_completed(futures_map):
                r = future.result()
                if r:
                    r["rollouts"] = rollouts
                    results[rollouts].append(r)
                with done_lock:
                    done += 1
                    with _print_lock:
                        print(f"  Progress: {done}/{total} games completed", flush=True)

    return results
 
# ── Plotting ───────────────────────────────────────────────────────────────────
 
PALETTE = plt.cm.plasma(np.linspace(0.15, 0.85, len(ROLLOUT_COUNTS)))
 
def plot_all(results: dict[int, list[dict]]):
    fig = plt.figure(figsize=(18, 14), facecolor="#0f0f14")
    fig.suptitle("2048 MCTS Benchmark", fontsize=22, color="white",
                 fontweight="bold", y=0.98)
 
    gs = GridSpec(2, 3, figure=fig, hspace=0.45, wspace=0.38,
                  left=0.07, right=0.97, top=0.92, bottom=0.08)
 
    ax_cdf    = fig.add_subplot(gs[0, 0:2])   # cumulative score distribution (wide)
    ax_box    = fig.add_subplot(gs[0, 2])     # box/violin scores
    ax_time   = fig.add_subplot(gs[1, 0])     # median time per game
    ax_mps    = fig.add_subplot(gs[1, 1])     # moves per second
    ax_tile   = fig.add_subplot(gs[1, 2])     # max tile frequency stacked bar
 
    bg = "#0f0f14"
    for ax in [ax_cdf, ax_box, ax_time, ax_mps, ax_tile]:
        ax.set_facecolor("#1a1a24")
        for spine in ax.spines.values():
            spine.set_edgecolor("#333348")
        ax.tick_params(colors="#aaaacc", labelsize=9)
        ax.xaxis.label.set_color("#ccccee")
        ax.yaxis.label.set_color("#ccccee")
        ax.title.set_color("white")
 
    rollout_levels = sorted(results.keys())
 
    # ── 1. Cumulative Score Distribution ──────────────────────────────────────
    score_max = max(r["score"] for rs in results.values() for r in rs)
    xs = np.linspace(0, score_max * 1.05, 500)
 
    for rollouts, color in zip(rollout_levels, PALETTE):
        scores = np.array([r["score"] for r in results[rollouts]])
        cdf = np.array([np.mean(scores <= x) for x in xs])
        ax_cdf.plot(xs, cdf * 100, color=color, linewidth=2.2,
                    label=f"{rollouts} rollouts (n={len(scores)})")
        # shade under curve lightly
        ax_cdf.fill_between(xs, cdf * 100, alpha=0.07, color=color)
 
    ax_cdf.set_title("Cumulative Score Distribution", fontsize=13, pad=8)
    ax_cdf.set_xlabel("Final Score")
    ax_cdf.set_ylabel("Games ≤ Score (%)")
    ax_cdf.set_ylim(0, 102)
    ax_cdf.xaxis.set_major_formatter(mticker.FuncFormatter(lambda x, _: f"{int(x):,}"))
    ax_cdf.legend(fontsize=8.5, framealpha=0.2, labelcolor="white",
                  facecolor="#1a1a24", edgecolor="#333348")
    ax_cdf.grid(alpha=0.15, color="#4444aa", linestyle="--")
 
    # ── 2. Box plots ──────────────────────────────────────────────────────────
    data_for_box = [np.array([r["score"] for r in results[k]]) for k in rollout_levels]
    bp = ax_box.boxplot(data_for_box, patch_artist=True, notch=False,
                        medianprops=dict(color="white", linewidth=2),
                        whiskerprops=dict(color="#8888aa"),
                        capprops=dict(color="#8888aa"),
                        flierprops=dict(marker="o", color="#ff6699",
                                        markerfacecolor="#ff6699", markersize=4))
    for patch, color in zip(bp["boxes"], PALETTE):
        patch.set_facecolor(color)
        patch.set_alpha(0.75)
 
    ax_box.set_title("Score Distribution", fontsize=13, pad=8)
    ax_box.set_xlabel("Max Rollouts")
    ax_box.set_ylabel("Final Score")
    ax_box.set_xticklabels([str(k) for k in rollout_levels], fontsize=8)
    ax_box.yaxis.set_major_formatter(mticker.FuncFormatter(lambda x, _: f"{int(x):,}"))
    ax_box.grid(axis="y", alpha=0.15, color="#4444aa", linestyle="--")
 
    # ── 3. Median time per game ───────────────────────────────────────────────
    medians_time = [np.median([r["seconds"] for r in results[k]]) for k in rollout_levels]
    bars = ax_time.bar(range(len(rollout_levels)), medians_time, color=PALETTE, alpha=0.85,
                       edgecolor="#0f0f14", linewidth=0.8)
    ax_time.set_title("Median Game Duration", fontsize=13, pad=8)
    ax_time.set_xlabel("Max Rollouts")
    ax_time.set_ylabel("Seconds")
    ax_time.set_xticks(range(len(rollout_levels)))
    ax_time.set_xticklabels([str(k) for k in rollout_levels], fontsize=8)
    ax_time.grid(axis="y", alpha=0.15, color="#4444aa", linestyle="--")
    for bar, val in zip(bars, medians_time):
        ax_time.text(bar.get_x() + bar.get_width() / 2, val + max(medians_time) * 0.01,
                     f"{val:.1f}s", ha="center", va="bottom", color="white", fontsize=8)
 
    # ── 4. Moves per second ───────────────────────────────────────────────────
    for rollouts, color in zip(rollout_levels, PALETTE):
        mps_vals = [r["moves_per_sec"] for r in results[rollouts]]
        ax_mps.scatter([rollouts] * len(mps_vals), mps_vals,
                       color=color, alpha=0.6, s=30, zorder=3)
        ax_mps.plot([rollouts], [np.median(mps_vals)], marker="D",
                    color=color, markersize=8, zorder=4,
                    markeredgecolor="white", markeredgewidth=0.8)
 
    ax_mps.set_title("Moves / Second (median = ◆)", fontsize=13, pad=8)
    ax_mps.set_xlabel("Max Rollouts")
    ax_mps.set_ylabel("Moves per Second")
    ax_mps.set_xscale("log")
    ax_mps.set_xticks(rollout_levels)
    ax_mps.get_xaxis().set_major_formatter(mticker.ScalarFormatter())
    ax_mps.grid(alpha=0.15, color="#4444aa", linestyle="--")
 
    # ── 5. Max tile reached (stacked bar) ─────────────────────────────────────
    common_tiles = [256, 512, 1024, 2048, 4096, 8192]
    tile_palette = plt.cm.YlOrRd(np.linspace(0.25, 0.95, len(common_tiles)))
    bottoms = np.zeros(len(rollout_levels))

    for tile_val, tc in zip(common_tiles, tile_palette):
        freqs = []
        for k in rollout_levels:
            scores_k = results[k]
            pct = 100 * sum(1 for r in scores_k if r["max_tile"] == tile_val) / max(len(scores_k), 1)
            freqs.append(pct)
        if any(f > 0 for f in freqs):
            ax_tile.bar(range(len(rollout_levels)), freqs, bottom=bottoms,
                        color=tc, alpha=0.85, label=f"{tile_val}",
                        edgecolor="#0f0f14", linewidth=0.6)
            bottoms = np.add(bottoms, freqs)
 
    ax_tile.set_title("Max Tile Reached (%)", fontsize=13, pad=8)
    ax_tile.set_xlabel("Max Rollouts")
    ax_tile.set_ylabel("% of games")
    ax_tile.set_xticks(range(len(rollout_levels)))
    ax_tile.set_xticklabels([str(k) for k in rollout_levels], fontsize=8)
    ax_tile.legend(fontsize=7.5, framealpha=0.2, labelcolor="white",
                   facecolor="#1a1a24", edgecolor="#333348", ncol=2)
    ax_tile.grid(axis="y", alpha=0.15, color="#4444aa", linestyle="--")
    ax_tile.set_ylim(0, 105)
 
    # ── Save ──────────────────────────────────────────────────────────────────
    out = Path("mcts_benchmark.png")
    fig.savefig(out, dpi=150, bbox_inches="tight", facecolor=bg)
    print(f"\n✓ Plot saved to {out.resolve()}")
    plt.show()
 
 
# ── Summary table ─────────────────────────────────────────────────────────────
 
def print_summary(results: dict[int, list[dict]]):
    print("\n" + "═" * 72)
    print(f"{'Rollouts':>10} │ {'N':>4} │ {'Mean Score':>12} │ {'Median':>10} │ "
          f"{'Std':>10} │ {'Median Time':>12}")
    print("─" * 72)
    for k in sorted(results.keys()):
        rs = results[k]
        scores = [r["score"]   for r in rs]
        times  = [r["seconds"] for r in rs]
        print(f"{k:>10} │ {len(rs):>4} │ {np.mean(scores):>12,.0f} │ "
              f"{np.median(scores):>10,.0f} │ {np.std(scores):>10,.0f} │ "
              f"{np.median(times):>10.1f}s")
    print("═" * 72)
 
 
# ── Entry point ───────────────────────────────────────────────────────────────
 
if __name__ == "__main__":
    print("2048 MCTS Benchmarker")
    print(f"  Executable : {EXECUTABLE}")
    print(f"  Rollouts   : {ROLLOUT_COUNTS}")
    print(f"  Runs/level : {RUNS_PER_LEVEL}")
    print(f"  Workers    : {MAX_WORKERS}")
    print(f"  Total games: {len(ROLLOUT_COUNTS) * RUNS_PER_LEVEL}")
    print()
 
    if not Path(EXECUTABLE).exists() and not any(
        (Path(p) / EXECUTABLE.lstrip("./")).exists()
        for p in ["."] + sys.path
    ):
        found = list(Path(".").glob("**/" + Path(EXECUTABLE).name))
        if found:
            EXECUTABLE = str(found[0])
            print(f"  Found binary at: {EXECUTABLE}")
        else:
            print(f"WARNING: '{EXECUTABLE}' not found in current directory.")
            print("  Set EXECUTABLE at the top of this script and rerun.")
            print("  Also make sure your C++ main() reads argv[1] for rollout count.")
            sys.exit(1)
 
    results = run_benchmark()
    print_summary(results)
    plot_all(results)