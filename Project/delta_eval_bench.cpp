/**
 * @file delta_eval_bench.cpp
 * @brief Roofline benchmark for VRP delta evaluation (insertion cost).
 *
 * Measures two variants:
 *  - NAIVE:       computes dist on-the-fly via sqrt(dx²+dy²)
 *  - OPTIMIZED:   reads from a pre-computed flat distance matrix
 *
 * For each variant records:
 *  - wall-clock time (seconds)
 *  - throughput (MFLOP/s)
 *  - memory bandwidth (GB/s)
 *  - arithmetic intensity (FLOP/byte)
 *
 * Hardware peak values for Apple M1 Pro are embedded as constants and
 * also written to the CSV so the Python plotter can draw Roofline lines
 * without needing platform detection.
 *
 * Output: results.csv  (same directory as the binary)
 *
 * Build:
 *   clang++ -std=c++17 -O2 -o delta_eval_bench delta_eval_bench.cpp
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
// Theoretical peak: 8-wide NEON FP64, 3.2 GHz → 8 × 3.2 = 25.6 GFLOP/s
// Memory bandwidth (LPDDR5): ~68 GB/s (measured)
// L2 bandwidth: ~250 GB/s (Apple Instruments / stream benchmark)
// L1 bandwidth: ~800 GB/s
static constexpr double HW_PEAK_FLOPS_GFLOPS   = 25.6;   // GFLOP/s (scalar DP)
static constexpr double HW_PEAK_MEM_BW_GBS      = 68.0;   // GB/s  DRAM
static constexpr double HW_PEAK_L2_BW_GBS       = 250.0;  // GB/s  L2 cache
static constexpr double HW_PEAK_L1_BW_GBS       = 800.0;  // GB/s  L1 cache

// ── Helpers ──────────────────────────────────────────────────────────────────

using Clock = std::chrono::steady_clock;

inline double elapsed_sec(Clock::time_point t0, Clock::time_point t1) {
    return std::chrono::duration<double>(t1 - t0).count();
}

/// Euclidean distance between two points.
inline double euclidean(double x1, double y1, double x2, double y2) {
    double dx = x1 - x2, dy = y1 - y2;
    return std::sqrt(dx * dx + dy * dy);
}

// ── Data generation ──────────────────────────────────────────────────────────

struct Client {
    double x, y;
};

/// Generate N random clients in [0, 1000]².
std::vector<Client> generate_clients(int n, unsigned seed = 42) {
    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<double> dist(0.0, 1000.0);
    std::vector<Client> clients(n);
    for (auto& c : clients) { c.x = dist(rng); c.y = dist(rng); }
    return clients;
}

/// Build flat row-major distance matrix (n×n doubles).
std::vector<double> build_dist_matrix(const std::vector<Client>& clients) {
    int n = static_cast<int>(clients.size());
    std::vector<double> mat(static_cast<std::size_t>(n) * n);
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            mat[static_cast<std::size_t>(i) * n + j] =
                euclidean(clients[i].x, clients[i].y,
                          clients[j].x, clients[j].y);
    return mat;
}

/// Build random routes (each route is a vector of client indices 1..N-1).
std::vector<std::vector<int>> generate_routes(int n_clients, int n_routes,
                                               unsigned seed = 7) {
    std::vector<int> perm(n_clients - 1);          // skip depot (index 0)
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
 * NAIVE variant:
 *   For every candidate insertion (client c between positions j and k in every
 *   route), compute delta = dist(j,c) + dist(c,k) - dist(j,k) by calling
 *   sqrt() three times.
 *
 * FLOP count per evaluation:
 *   2 subtractions + 2 multiplications + 1 addition = 5 FLOPs  ×3 sqrt
 *   + 3 sqrt(~20 FLOPs each, hardware estimate) + 2 add/sub = ~75 FLOPs total
 *   We use a conservative 75 FLOPs per delta.
 *
 * Memory:
 *   Reads x,y for 3 clients = 6 doubles = 48 bytes per delta.
 */
double bench_naive(const std::vector<Client>& clients,
                   const std::vector<std::vector<int>>& routes,
                   int repeats,
                   double& out_time_sec,
                   int64_t& out_total_evals) {
    int n = static_cast<int>(clients.size());
    volatile double sink = 0.0;   // prevent dead-code elimination
    int64_t evals = 0;

    auto t0 = Clock::now();
    for (int rep = 0; rep < repeats; ++rep) {
        for (int c = 1; c < n; ++c) {                       // candidate client
            for (const auto& route : routes) {
                int sz = static_cast<int>(route.size());
                for (int pos = 0; pos <= sz; ++pos) {
                    // predecessor: route[pos-1] or depot(0)
                    int j = (pos == 0) ? 0 : route[pos - 1];
                    // successor:   route[pos]   or depot(0)
                    int k = (pos == sz) ? 0 : route[pos];

                    double dj = euclidean(clients[j].x, clients[j].y,
                                         clients[c].x, clients[c].y);
                    double dk = euclidean(clients[c].x, clients[c].y,
                                         clients[k].x, clients[k].y);
                    double djk = euclidean(clients[j].x, clients[j].y,
                                          clients[k].x, clients[k].y);
                    double delta = dj + dk - djk;
                    sink += delta;
                    ++evals;
                }
            }
        }
    }
    auto t1 = Clock::now();

    out_time_sec   = elapsed_sec(t0, t1);
    out_total_evals = evals;
    return static_cast<double>(sink);   // return to avoid optimisation
}

/**
 * OPTIMIZED variant:
 *   Reads delta from pre-computed matrix:
 *     delta = mat[j*n+c] + mat[c*n+k] - mat[j*n+k]
 *   Three reads from the matrix, two additions, one subtraction.
 *
 * FLOP count: 2 FLOPs (add + sub) per delta.
 *
 * Memory: 3 doubles read per delta = 24 bytes (if not cached).
 *   In practice the matrix is accessed with varying locality, so effective
 *   bandwidth sits between L2 and DRAM.
 */
double bench_optimized(const std::vector<double>& mat,
                       int n_clients,
                       const std::vector<std::vector<int>>& routes,
                       int repeats,
                       double& out_time_sec,
                       int64_t& out_total_evals) {
    int n = n_clients;
    volatile double sink = 0.0;
    int64_t evals = 0;

    auto t0 = Clock::now();
    for (int rep = 0; rep < repeats; ++rep) {
        for (int c = 1; c < n; ++c) {
            for (const auto& route : routes) {
                int sz = static_cast<int>(route.size());
                for (int pos = 0; pos <= sz; ++pos) {
                    int j = (pos == 0) ? 0 : route[pos - 1];
                    int k = (pos == sz) ? 0 : route[pos];

                    double delta = mat[static_cast<std::size_t>(j) * n + c]
                                 + mat[static_cast<std::size_t>(c) * n + k]
                                 - mat[static_cast<std::size_t>(j) * n + k];
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

/**
 * Measure achieved DRAM bandwidth by copying a large array.
 * Returns GB/s.
 */
double measure_bandwidth_gbs(std::size_t array_size_mb = 256) {
    std::size_t n = (array_size_mb * 1024ULL * 1024ULL) / sizeof(double);
    std::vector<double> src(n, 1.0), dst(n, 0.0);

    // Warm-up
    std::copy(src.begin(), src.end(), dst.begin());

    auto t0 = Clock::now();
    constexpr int iters = 5;
    for (int i = 0; i < iters; ++i)
        std::copy(src.begin(), src.end(), dst.begin());
    auto t1 = Clock::now();

    double bytes  = static_cast<double>(n) * sizeof(double) * 2 * iters; // read+write
    double sec    = elapsed_sec(t0, t1);
    return (bytes / sec) / 1e9;
}

// ── CSV output ────────────────────────────────────────────────────────────────

void write_csv(const std::string& path,
               int n_clients, int n_routes, int repeats,
               double meas_bw_gbs,
               // naive
               double naive_time, int64_t naive_evals,
               double naive_ai, double naive_gflops,
               double naive_bw_gbs,
               // optimized
               double opt_time, int64_t opt_evals,
               double opt_ai, double opt_gflops,
               double opt_bw_gbs)
{
    std::ofstream f(path);
    if (!f) { std::cerr << "Cannot open " << path << "\n"; return; }

    // ── Hardware section ──────────────────────────────────────────────────────
    f << "# Hardware constants (Apple M1 Pro)\n";
    f << "hw_peak_flops_gflops,"  << HW_PEAK_FLOPS_GFLOPS  << "\n";
    f << "hw_peak_mem_bw_gbs,"   << HW_PEAK_MEM_BW_GBS    << "\n";
    f << "hw_peak_l2_bw_gbs,"    << HW_PEAK_L2_BW_GBS     << "\n";
    f << "hw_peak_l1_bw_gbs,"    << HW_PEAK_L1_BW_GBS     << "\n";
    f << "measured_mem_bw_gbs,"  << meas_bw_gbs            << "\n";
    f << "\n";

    // ── Benchmark parameters ──────────────────────────────────────────────────
    f << "# Benchmark parameters\n";
    f << "n_clients," << n_clients << "\n";
    f << "n_routes,"  << n_routes  << "\n";
    f << "repeats,"   << repeats   << "\n";
    f << "\n";

    // ── Per-variant results ───────────────────────────────────────────────────
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
    // Parse optional arguments
    int n_clients = (argc > 1) ? std::atoi(argv[1]) : 500;
    int n_routes  = (argc > 2) ? std::atoi(argv[2]) : 20;
    int repeats   = (argc > 3) ? std::atoi(argv[3]) : 200;

    std::cout << "=== VRP Delta Evaluation Roofline Benchmark ===\n";
    std::cout << "Platform: Apple M1 Pro (constants embedded)\n";
    std::cout << "N clients: " << n_clients
              << "  routes: " << n_routes
              << "  repeats: " << repeats << "\n\n";

    // Generate data
    auto clients = generate_clients(n_clients);
    auto mat     = build_dist_matrix(clients);
    auto routes  = generate_routes(n_clients, n_routes);

    // Measure memory bandwidth
    std::cout << "[1/3] Measuring DRAM bandwidth (STREAM copy, 256 MB)...\n";
    double meas_bw = measure_bandwidth_gbs(256);
    std::cout << "      Achieved: " << meas_bw << " GB/s\n\n";

    // ── NAIVE ────────────────────────────────────────────────────────────────
    std::cout << "[2/3] Running NAIVE benchmark...\n";
    double naive_time; int64_t naive_evals;
    bench_naive(clients, routes, repeats, naive_time, naive_evals);

    //  75 FLOPs per eval (5 FLOPs for arithmetic ×3 pairs + ~60 FLOPs sqrt×3)
    constexpr double NAIVE_FLOPS_PER_EVAL = 75.0;
    // Memory: 3 client coords pairs × 2 doubles × 3 pairs = 6 doubles per eval
    //   but each client has x and y, so 3 clients × 2 × 8 bytes = 48 bytes
    constexpr double NAIVE_BYTES_PER_EVAL = 48.0;

    double naive_total_flops = static_cast<double>(naive_evals) * NAIVE_FLOPS_PER_EVAL;
    double naive_total_bytes = static_cast<double>(naive_evals) * NAIVE_BYTES_PER_EVAL;
    double naive_ai     = naive_total_flops / naive_total_bytes;      // FLOP/byte
    double naive_gflops = (naive_total_flops / naive_time) / 1e9;
    double naive_bw_gbs = (naive_total_bytes / naive_time) / 1e9;

    std::cout << "      Time:       " << naive_time  << " s\n"
              << "      Evals:      " << naive_evals << "\n"
              << "      AI:         " << naive_ai    << " FLOP/byte\n"
              << "      Throughput: " << naive_gflops << " GFLOP/s\n"
              << "      Bandwidth:  " << naive_bw_gbs << " GB/s\n\n";

    // ── OPTIMIZED ────────────────────────────────────────────────────────────
    std::cout << "[3/3] Running OPTIMIZED benchmark...\n";
    double opt_time; int64_t opt_evals;
    bench_optimized(mat, n_clients, routes, repeats, opt_time, opt_evals);

    //  2 FLOPs per eval (add + sub)
    constexpr double OPT_FLOPS_PER_EVAL = 2.0;
    // Memory: 3 doubles read = 24 bytes per eval (upper bound; often in cache)
    constexpr double OPT_BYTES_PER_EVAL = 24.0;

    double opt_total_flops = static_cast<double>(opt_evals) * OPT_FLOPS_PER_EVAL;
    double opt_total_bytes = static_cast<double>(opt_evals) * OPT_BYTES_PER_EVAL;
    double opt_ai     = opt_total_flops / opt_total_bytes;
    double opt_gflops = (opt_total_flops / opt_time) / 1e9;
    double opt_bw_gbs = (opt_total_bytes / opt_time) / 1e9;

    std::cout << "      Time:       " << opt_time    << " s\n"
              << "      Evals:      " << opt_evals   << "\n"
              << "      AI:         " << opt_ai      << " FLOP/byte\n"
              << "      Throughput: " << opt_gflops  << " GFLOP/s\n"
              << "      Bandwidth:  " << opt_bw_gbs  << " GB/s\n\n";

    // ── Write CSV ─────────────────────────────────────────────────────────────
    write_csv("results.csv",
              n_clients, n_routes, repeats, meas_bw,
              naive_time, naive_evals, naive_ai, naive_gflops, naive_bw_gbs,
              opt_time,   opt_evals,   opt_ai,   opt_gflops,   opt_bw_gbs);

    return 0;
}
