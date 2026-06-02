#!/usr/bin/env bash
# =============================================================================
# run_bench.sh — Build and run the VRP delta-evaluation Roofline benchmark
# Platform: macOS (Apple Silicon / M1 Pro)
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="$SCRIPT_DIR/delta_eval_bench.cpp"
BIN="$SCRIPT_DIR/delta_eval_bench"
CSV="$SCRIPT_DIR/results.csv"

# ── Compiler selection ────────────────────────────────────────────────────────
# Prefer clang++ (ships with Xcode CLT on macOS), fall back to g++
if command -v clang++ &>/dev/null; then
    CXX="clang++"
elif command -v g++ &>/dev/null; then
    CXX="g++"
else
    echo "ERROR: No C++ compiler found. Install Xcode Command Line Tools:" >&2
    echo "  xcode-select --install" >&2
    exit 1
fi

echo "=== Compiler: $CXX ==="

# ── Build ─────────────────────────────────────────────────────────────────────
# -O2           : standard optimisation (matches typical project build)
# -std=c++17    : required by project spec
# -march=native : allow compiler to use NEON / AMX on M1
# -ffast-math   : let compiler vectorise sqrt chains (optional, disables IEEE strict)
echo "Building $SRC ..."
$CXX -std=c++17 -O2 -march=native -ffast-math \
     -Wall -Wextra \
     -o "$BIN" "$SRC"
echo "Build OK → $BIN"
echo ""

# ── Run benchmark ─────────────────────────────────────────────────────────────
# Arguments: [N_clients] [N_routes] [repeats]
# Defaults give ~30-60 s total runtime on M1 Pro.
N_CLIENTS="${1:-500}"
N_ROUTES="${2:-20}"
REPEATS="${3:-200}"

echo "Running benchmark: N=$N_CLIENTS  routes=$N_ROUTES  repeats=$REPEATS"
"$BIN" "$N_CLIENTS" "$N_ROUTES" "$REPEATS"
echo ""

# ── Plot ──────────────────────────────────────────────────────────────────────
PLOT_SCRIPT="$SCRIPT_DIR/plot_roofline.py"
if [[ -f "$PLOT_SCRIPT" ]]; then
    echo "Plotting Roofline model ..."
    python3 "$PLOT_SCRIPT" "$CSV"
else
    echo "plot_roofline.py not found; skipping plot."
fi