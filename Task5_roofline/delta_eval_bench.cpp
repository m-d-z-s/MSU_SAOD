/**
 * 
 * @file delta_eval_bench.cpp
 * @brief Roofline benchmark for VRP delta evaluation (insertion cost).
 *
 * Compares two variants that perform IDENTICAL mathematical work
 * (three Euclidean distances per evaluation), but differ only in
 * how client coordinate data is organised in memory:
 *
 *  - NAIVE (AoS):      struct Client { double x, y; }
 *                      Array-of-Structs layout — x and y interleaved.
 *                      Poor SIMD vectorisation: loads stride 2 doubles.
 *
 *  - OPTIMIZED (SoA):  std::vector<double> xs, ys;
 *                      Structure-of-Arrays layout — all x contiguous,
 *                      all y contiguous.
 *                      Better vectorisation: loads are sequential.
 *
 * Both variants compute exactly the same formula per evaluation:
 *   delta = dist(j,c) + dist(c,k) - dist(j,k)
 * where dist(a,b) = sqrt((xa-xb)^2 + (ya-yb)^2)
 *
 * FLOP count is identical for both variants:
 *   Per distance: 2 sub + 2 mul + 1 add + 1 sqrt ≈ 25 FLOPs (sqrt~20)
 *   Per eval (3 distances): 75 FLOPs + 1 add + 1 sub = 77 FLOPs
 *
 * Memory traffic per evaluation (3 pairs of clients, each x+y = 16 bytes):
 *   We read coords for clients j, c, k — but c is the outer loop,
 *   so x[c], y[c] are typically cached across the inner loops.
 *   Pessimistic bound: 3 clients × 16 bytes = 48 bytes per eval.
 *
 * Output: results.csv  (same directory as the binary)
 *
 * Build:
 *   clang++ -std=c++17 -O2 -march=native -ffast-math -o delta_eval_bench delta_eval_bench.cpp
 *
 * Usage:
 *   ./delta_eval_bench [N_clients] [N_routes] [repeats]
 *   Defaults: N=500, R=20, repeats=200
 */

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <vector>

// ── Apple M1 Pro hardware constants ─────────────────────────────────────────
static constexpr double HW_PEAK_FLOPS_GFLOPS = 25.6;   // GFLOP/s (NEON DP)
static constexpr double HW_PEAK_MEM_BW_GBS   = 68.0;   // GB/s  DRAM theoretical
static constexpr double HW_PEAK_L2_BW_GBS    = 250.0;  // GB/s  L2 cache
static constexpr double HW_PEAK_L1_BW_GBS    = 800.0;  // GB/s  L1 cache


static constexpr double FLOPS_PER_EVAL = 77.0;

// Memory bytes per evaluation:
//   3 clients (j, c, k) × (x + y) = 3 × 2 × 8 = 48 bytes
static constexpr double BYTES_PER_EVAL = 48.0;

// ── Helpers ──────────────────────────────────────────────────────────────────

using Clock = std::chrono::steady_clock;

inline double elapsed_sec(Clock::time_point t0, Clock::time_point t1) {
    return std::chrono::duration<double>(t1 - t0).count();
}

// ── Data structures ──────────────────────────────────────────────────────────

/**
 * NAIVE layout: Array of Structs (AoS).
 * Memory: x0 y0 | x1 y1 | x2 y2 | ...
 * When the inner loop iterates over different j/k clients,
 * the CPU loads stride-16-byte pairs — hard to auto-vectorise.
 */
struct Client {
    double x, y;
};

/**
 * OPTIMIZED layout: Structure of Arrays (SoA).
 * Memory: x0 x1 x2 ... xN | y0 y1 y2 ... yN
 * Sequential access to xs[] and ys[] enables auto-vectorisation
 */
struct ClientSoA {
    std::vector<double> xs, ys;
    explicit ClientSoA(int n) : xs(n), ys(n) {}
};

// ── Data generation ──────────────────────────────────────────────────────────

/// Generate N random clients — AoS layout.
std::vector<Client> generate_clients_aos(int n, unsigned seed = 42) {
    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<double> dist(0.0, 1000.0);
    std::vector<Client> clients(n);
    for (auto& c : clients) { c.x = dist(rng); c.y = dist(rng); }
    return clients;
}

/// Build SoA from AoS (same coordinates, different layout).
ClientSoA aos_to_soa(const std::vector<Client>& aos) {
    ClientSoA soa(static_cast<int>(aos.size()));
    for (std::size_t i = 0; i < aos.size(); ++i) {
        soa.xs[i] = aos[i].x;
        soa.ys[i] = aos[i].y;
    }
    return soa;
}

/// Build random routes (each route is a vector of client indices 1..N-1).
std::vector<std::vector<int>> generate_routes(int n_clients, int n_routes,
                                               unsigned seed = 7) {
    std::vector<int> perm(n_clients - 1);
    std::iota(perm.begin(), perm.end(), 1);
    std::mt19937 rng(seed);
    std::shuffle(perm.begin(), perm.end(), rng);

    std::vector<std::vector<int>> routes(n_routes);
    for (int i = 0; i < static_cast<int>(perm.size()); ++i)
        routes[i % n_routes].push_back(perm[i]);
    return routes;
}

// ── Benchmark kernels ─────────────────────────────────────────────────────────

/**
 * NAIVE variant — AoS layout.
 */
double bench_naive_aos(const std::vector<Client>& clients,
                       const std::vector<std::vector<int>>& routes,
                       int repeats,
                       double& out_time_sec,
                       int64_t& out_total_evals) {
    int n = static_cast<int>(clients.size());
    volatile double sink = 0.0;
    int64_t evals = 0;

    auto t0 = Clock::now();
    for (int rep = 0; rep < repeats; ++rep) {
        for (int c = 1; c < n; ++c) {
            const double cx = clients[c].x, cy = clients[c].y;
            for (const auto& route : routes) {
                int sz = static_cast<int>(route.size());
                for (int pos = 0; pos <= sz; ++pos) {
                    int j = (pos == 0)  ? 0 : route[pos - 1];
                    int k = (pos == sz) ? 0 : route[pos];

                    // dist(j, c)
                    double dx_jc = clients[j].x - cx;
                    double dy_jc = clients[j].y - cy;
                    double d_jc  = std::sqrt(dx_jc * dx_jc + dy_jc * dy_jc);

                    // dist(c, k)
                    double dx_ck = cx - clients[k].x;
                    double dy_ck = cy - clients[k].y;
                    double d_ck  = std::sqrt(dx_ck * dx_ck + dy_ck * dy_ck);

                    // dist(j, k)
                    double dx_jk = clients[j].x - clients[k].x;
                    double dy_jk = clients[j].y - clients[k].y;
                    double d_jk  = std::sqrt(dx_jk * dx_jk + dy_jk * dy_jk);

                    double delta = d_jc + d_ck - d_jk;
                    sink += delta;
                    ++evals;
                }
            }
        }
    }
    auto t1 = Clock::now();

    out_time_sec    = elapsed_sec(t0, t1);
    out_total_evals = evals;
    return static_cast<double>(sink);
}

/**
 * OPTIMIZED variant — SoA layout.
 *
 * Mathematical work is IDENTICAL to the AoS variant:
 *   delta = dist(j,c) + dist(c,k) - dist(j,k)  via three sqrts.
 */
double bench_optimized_soa(const ClientSoA& soa,
                           const std::vector<std::vector<int>>& routes,
                           int repeats,
                           double& out_time_sec,
                           int64_t& out_total_evals) {
    int n = static_cast<int>(soa.xs.size());
    const double* xs = soa.xs.data();
    const double* ys = soa.ys.data();
    volatile double sink = 0.0;
    int64_t evals = 0;

    auto t0 = Clock::now();
    for (int rep = 0; rep < repeats; ++rep) {
        for (int c = 1; c < n; ++c) {
            const double cx = xs[c], cy = ys[c];  // cached in register
            for (const auto& route : routes) {
                int sz = static_cast<int>(route.size());
                for (int pos = 0; pos <= sz; ++pos) {
                    int j = (pos == 0)  ? 0 : route[pos - 1];
                    int k = (pos == sz) ? 0 : route[pos];

                    // dist(j, c) — reads xs[j], ys[j] from SoA arrays
                    double dx_jc = xs[j] - cx;
                    double dy_jc = ys[j] - cy;
                    double d_jc  = std::sqrt(dx_jc * dx_jc + dy_jc * dy_jc);

                    // dist(c, k) — reads xs[k], ys[k] from SoA arrays
                    double dx_ck = cx - xs[k];
                    double dy_ck = cy - ys[k];
                    double d_ck  = std::sqrt(dx_ck * dx_ck + dy_ck * dy_ck);

                    // dist(j, k) — reads xs[j], ys[j], xs[k], ys[k]
                    double dx_jk = xs[j] - xs[k];
                    double dy_jk = ys[j] - ys[k];
                    double d_jk  = std::sqrt(dx_jk * dx_jk + dy_jk * dy_jk);

                    double delta = d_jc + d_ck - d_jk;
                    sink += delta;
                    ++evals;
                }
            }
        }
    }
    auto t1 = Clock::now();

    out_time_sec    = elapsed_sec(t0, t1);
    out_total_evals = evals;
    return static_cast<double>(sink);
}

// ── Memory bandwidth measurement (STREAM-like copy) ──────────────────────────

double measure_bandwidth_gbs(std::size_t array_size_mb = 256) {
    std::size_t n = (array_size_mb * 1024ULL * 1024ULL) / sizeof(double);
    std::vector<double> src(n, 1.0), dst(n, 0.0);

    // Warm-up pass
    for (std::size_t i = 0; i < n; ++i) dst[i] = src[i];

    volatile double sink = dst[n / 2];  // prevent DCE of warm-up

    auto t0 = Clock::now();
    constexpr int iters = 5;
    for (int it = 0; it < iters; ++it)
        for (std::size_t i = 0; i < n; ++i)
            dst[i] = src[i];
    auto t1 = Clock::now();

    sink = dst[n / 2];  // force use of dst to prevent compiler DCE
    (void)sink;

    double bytes = static_cast<double>(n) * sizeof(double) * 2.0 * iters;
    double sec   = elapsed_sec(t0, t1);
    return (sec > 0.0) ? (bytes / sec) / 1e9 : 60.0;  // fallback if DCE
}

// ── CSV output ────────────────────────────────────────────────────────────────

void write_csv(const std::string& path,
               int n_clients, int n_routes, int repeats,
               double meas_bw_gbs,
               double naive_time, int64_t naive_evals,
               double naive_ai, double naive_gflops, double naive_bw_gbs,
               double opt_time,   int64_t opt_evals,
               double opt_ai,     double opt_gflops,   double opt_bw_gbs)
{
    std::ofstream f(path);
    if (!f) { std::cerr << "Cannot open " << path << "\n"; return; }

    f << "# Hardware constants (Apple M1 Pro)\n";
    f << "hw_peak_flops_gflops,"  << HW_PEAK_FLOPS_GFLOPS << "\n";
    f << "hw_peak_mem_bw_gbs,"   << HW_PEAK_MEM_BW_GBS   << "\n";
    f << "hw_peak_l2_bw_gbs,"    << HW_PEAK_L2_BW_GBS    << "\n";
    f << "hw_peak_l1_bw_gbs,"    << HW_PEAK_L1_BW_GBS    << "\n";
    f << "measured_mem_bw_gbs,"  << meas_bw_gbs           << "\n";
    f << "\n";

    f << "# Benchmark parameters\n";
    f << "n_clients," << n_clients << "\n";
    f << "n_routes,"  << n_routes  << "\n";
    f << "repeats,"   << repeats   << "\n";
    f << "\n";

    f << "# variant,time_sec,total_evals,arith_intensity_flop_byte,"
         "throughput_gflops,achieved_bw_gbs\n";
    f << "naive,"
      << naive_time  << "," << naive_evals << ","
      << naive_ai    << "," << naive_gflops << "," << naive_bw_gbs << "\n";
    f << "optimized,"
      << opt_time    << "," << opt_evals   << ","
      << opt_ai      << "," << opt_gflops  << "," << opt_bw_gbs    << "\n";

    f.close();
    std::cout << "Results written to: " << path << "\n";
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    int n_clients = (argc > 1) ? std::atoi(argv[1]) : 500;
    int n_routes  = (argc > 2) ? std::atoi(argv[2]) : 20;
    int repeats   = (argc > 3) ? std::atoi(argv[3]) : 200;

    std::cout << "=== VRP Delta Evaluation Roofline Benchmark ===\n";
    std::cout << "Platform : Apple M1 Pro (constants embedded)\n";
    std::cout << "Layout   : AoS (naive) vs SoA (optimized)\n";
    std::cout << "Math     : identical — 3x sqrt per delta evaluation\n";
    std::cout << "N clients: " << n_clients
              << "  routes: " << n_routes
              << "  repeats: " << repeats << "\n\n";

    // Generate data in AoS layout, then convert to SoA
    auto clients_aos = generate_clients_aos(n_clients);
    auto clients_soa = aos_to_soa(clients_aos);
    auto routes      = generate_routes(n_clients, n_routes);

    // Measure DRAM bandwidth
    std::cout << "[1/3] Measuring DRAM bandwidth (STREAM copy, 256 MB)...\n";
    double meas_bw = measure_bandwidth_gbs(256);
    std::cout << "      Achieved: " << meas_bw << " GB/s\n\n";

    // ── NAIVE (AoS) ─────────────────────────────────────────────────────────
    std::cout << "[2/3] Running NAIVE (AoS) benchmark...\n";
    double naive_time; int64_t naive_evals;
    bench_naive_aos(clients_aos, routes, repeats, naive_time, naive_evals);

    double naive_total_flops = static_cast<double>(naive_evals) * FLOPS_PER_EVAL;
    double naive_total_bytes = static_cast<double>(naive_evals) * BYTES_PER_EVAL;
    double naive_ai     = naive_total_flops / naive_total_bytes;
    double naive_gflops = (naive_total_flops / naive_time) / 1e9;
    double naive_bw_gbs = (naive_total_bytes / naive_time) / 1e9;

    std::cout << "      Time:       " << naive_time   << " s\n"
              << "      Evals:      " << naive_evals  << "\n"
              << "      AI:         " << naive_ai     << " FLOP/byte\n"
              << "      Throughput: " << naive_gflops << " GFLOP/s\n"
              << "      Bandwidth:  " << naive_bw_gbs << " GB/s\n\n";

    // ── OPTIMIZED (SoA) ─────────────────────────────────────────────────────
    std::cout << "[3/3] Running OPTIMIZED (SoA) benchmark...\n";
    double opt_time; int64_t opt_evals;
    bench_optimized_soa(clients_soa, routes, repeats, opt_time, opt_evals);

    double opt_total_flops = static_cast<double>(opt_evals) * FLOPS_PER_EVAL;
    double opt_total_bytes = static_cast<double>(opt_evals) * BYTES_PER_EVAL;
    double opt_ai     = opt_total_flops / opt_total_bytes;
    double opt_gflops = (opt_total_flops / opt_time) / 1e9;
    double opt_bw_gbs = (opt_total_bytes / opt_time) / 1e9;

    std::cout << "      Time:       " << opt_time    << " s\n"
              << "      Evals:      " << opt_evals   << "\n"
              << "      AI:         " << opt_ai      << " FLOP/byte\n"
              << "      Throughput: " << opt_gflops  << " GFLOP/s\n"
              << "      Bandwidth:  " << opt_bw_gbs  << " GB/s\n\n";

    // ── Summary ──────────────────────────────────────────────────────────────
    std::cout << "=== Summary ===\n";
    std::cout << "Speedup (SoA vs AoS): " << naive_time / opt_time << "x\n";
    std::cout << "GFLOP/s gain:         " << opt_gflops / naive_gflops << "x\n";
    std::cout << "AI is the same:       " << naive_ai << " == " << opt_ai
              << " FLOP/byte\n\n";

    write_csv("results.csv",
              n_clients, n_routes, repeats, meas_bw,
              naive_time, naive_evals, naive_ai, naive_gflops, naive_bw_gbs,
              opt_time,   opt_evals,   opt_ai,   opt_gflops,   opt_bw_gbs);

    return 0;
}