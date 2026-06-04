/**
 * @file run_benchmarks.cpp
 * @brief Прогон алгоритмов на синтетических инстансах с выводом результатов в CSV.
 *
 * Запуск:
 *   run_benchmarks [output.csv]
 *
 * Без аргументов — вывод в stdout.
 * Генерирует синтетические инстансы (равномерное и кластерное распределение)
 * и сравнивает все алгоритмы: NN, CW, CW+2opt, SA, TS, Hybrid.
 *
 * Формат CSV:
 *   instance,n_clients,capacity,algorithm,routes,total_length,
 *   score,valid,time_ms,distribution
 */

#include "core/distance.hpp"
#include "core/instance.hpp"
#include "core/solution.hpp"
#include "heuristics/nearest_neighbor.hpp"
#include "heuristics/clarke_wright.hpp"
#include "local_search/two_opt.hpp"
#include "local_search/or_opt.hpp"
#include "local_search/inter_route.hpp"
#include "metaheuristics/simulated_annealing.hpp"
#include "metaheuristics/tabu_search.hpp"
#include "hybrid/hybrid_solver.hpp"

#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using namespace vrp;

// ─── генераторы инстансов ────────────────────────────────────────────────────

/**
 * @brief Создаёт синтетический инстанс с равномерным распределением клиентов.
 *
 * Координаты — равномерно в [0, 100] × [0, 100].
 * Спрос — равномерно в [1, max_demand].
 * Вместимость выбрана так, чтобы в среднем было ~n/k клиентов на маршрут.
 *
 * @param n          Число клиентов.
 * @param capacity   Вместимость ТС.
 * @param seed       Начало ГСЧ.
 * @return Синтетический инстанс.
 */
static Instance make_uniform(int n, int capacity, unsigned seed = 0) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> coord(0.0, 100.0);
    std::uniform_int_distribution<int>     dem(1, 10);

    Instance inst;
    inst.name     = "uniform_n" + std::to_string(n);
    inst.capacity = capacity;
    inst.num_vehicles = 0;

    // Депо в центре
    inst.clients.push_back({0, 50.0, 50.0, 0, 0, 0, 0});
    for (int i = 1; i <= n; ++i)
        inst.clients.push_back({i, coord(rng), coord(rng), dem(rng), 0, 0, 0});

    return inst;
}

/**
 * @brief Создаёт синтетический инстанс с кластерным распределением клиентов.
 *
 * Клиенты сгруппированы в k кластеров; координаты — нормальные вокруг центра.
 *
 * @param n          Число клиентов.
 * @param capacity   Вместимость ТС.
 * @param n_clusters Число кластеров.
 * @param seed       Начало ГСЧ.
 * @return Синтетический инстанс.
 */
static Instance make_clustered(int n, int capacity,
                                int n_clusters = 3, unsigned seed = 0) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> center_dist(15.0, 85.0);
    std::normal_distribution<double>       spread(0.0, 8.0);
    std::uniform_int_distribution<int>     dem(1, 10);

    // Генерирует центры кластеров
    std::vector<std::pair<double, double>> centers;
    for (int c = 0; c < n_clusters; ++c)
        centers.push_back({center_dist(rng), center_dist(rng)});

    std::uniform_int_distribution<int> cluster_pick(0, n_clusters - 1);

    Instance inst;
    inst.name     = "cluster_n" + std::to_string(n);
    inst.capacity = capacity;
    inst.num_vehicles = 0;

    inst.clients.push_back({0, 50.0, 50.0, 0, 0, 0, 0});
    for (int i = 1; i <= n; ++i) {
        int  ci = cluster_pick(rng);
        double x = std::max(0.0, std::min(100.0, centers[ci].first  + spread(rng)));
        double y = std::max(0.0, std::min(100.0, centers[ci].second + spread(rng)));
        inst.clients.push_back({i, x, y, dem(rng), 0, 0, 0});
    }

    return inst;
}

// ─── структура для результатов ────────────────────────────────────────────────

struct BenchResult {
    std::string instance;
    int n_clients;
    int capacity;
    std::string algorithm;
    int routes;
    double total_length;
    double score;
    bool valid;
    double time_ms;
    std::string distribution;
};

// ─── прогон одного алгоритма ──────────────────────────────────────────────────

static BenchResult run_algorithm(const Instance& inst,
                                 const DistanceMatrix& dist,
                                 const std::string& algo_name,
                                 const std::string& distrib) {
    constexpr double kPenalty = 10.0;

    SAParams sa_params;
    sa_params.seed            = 42;
    sa_params.max_iter        = 20000;
    sa_params.vehicle_penalty = kPenalty;

    TSParams ts_params;
    ts_params.max_iter        = 300;
    ts_params.vehicle_penalty = kPenalty;

    HybridParams hybrid_params;
    hybrid_params.sa = sa_params;

    auto t0 = std::chrono::steady_clock::now();
    Solution sol;

    if (algo_name == "nn") {
        sol = NearestNeighbor::solve(inst, dist);
    } else if (algo_name == "cw") {
        sol = ClarkeWright::solve(inst, dist);
    } else if (algo_name == "cw2opt") {
        sol = ClarkeWright::solve(inst, dist);
        TwoOpt::improve(sol, inst, dist);
        OrOpt::improve(sol, inst, dist);
    } else if (algo_name == "sa") {
        sol = ClarkeWright::solve(inst, dist);
        TwoOpt::improve(sol, inst, dist);
        SimulatedAnnealing::optimize(sol, inst, dist, sa_params);
    } else if (algo_name == "ts") {
        sol = ClarkeWright::solve(inst, dist);
        TabuSearch::optimize(sol, inst, dist, ts_params);
    } else { // hybrid
        sol = HybridSolver::solve(inst, dist, hybrid_params);
    }

    auto t1 = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    BenchResult res;
    res.instance     = inst.name;
    res.n_clients    = inst.num_clients();
    res.capacity     = inst.capacity;
    res.algorithm    = algo_name;
    res.routes       = sol.num_routes();
    res.total_length = sol.total_length(dist);
    res.score        = HybridSolver::score(sol, dist, kPenalty);
    res.valid        = sol.validate(inst).empty();
    res.time_ms      = ms;
    res.distribution = distrib;
    return res;
}

// ─── main ─────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    // Выходной поток: файл или stdout
    std::ofstream file_out;
    std::ostream* out = &std::cout;
    if (argc >= 2) {
        file_out.open(argv[1]);
        if (!file_out.is_open()) {
            std::cerr << "Cannot open output file: " << argv[1] << "\n";
            return 1;
        }
        out = &file_out;
    }

    const std::vector<std::string> algorithms = {
        "nn", "cw", "cw2opt", "sa", "ts", "hybrid"
    };

    // Заголовок CSV
    *out << "instance,n_clients,capacity,algorithm,routes,"
         << "total_length,score,valid,time_ms,distribution\n";

    // Параметры тестов: (n_clients, capacity)
    const std::vector<std::pair<int,int>> configs = {
        {10, 30}, {20, 40}, {50, 50}, {100, 60}
    };

    std::vector<BenchResult> results;
    int total = 0;

    for (const auto& [n, cap] : configs) {
        std::cerr << "Testing n=" << n << " capacity=" << cap << "...\n";

        // Равномерное распределение
        for (unsigned seed : {0u, 1u, 2u}) {
            Instance inst_u = make_uniform(n, cap, seed);
            DistanceMatrix dist_u(inst_u);

            for (const auto& algo : algorithms) {
                auto res = run_algorithm(inst_u, dist_u, algo, "uniform");
                results.push_back(res);
                ++total;
            }
        }

        // Кластерное распределение
        for (unsigned seed : {0u, 1u, 2u}) {
            Instance inst_c = make_clustered(n, cap, 3, seed);
            DistanceMatrix dist_c(inst_c);

            for (const auto& algo : algorithms) {
                auto res = run_algorithm(inst_c, dist_c, algo, "clustered");
                results.push_back(res);
                ++total;
            }
        }
    }

    // Вывод результатов в CSV
    for (const auto& r : results) {
        *out << r.instance << ","
             << r.n_clients << ","
             << r.capacity << ","
             << r.algorithm << ","
             << r.routes << ","
             << r.total_length << ","
             << r.score << ","
             << (r.valid ? "true" : "false") << ","
             << r.time_ms << ","
             << r.distribution << "\n";
    }

    // Сводная статистика в stderr
    std::cerr << "\n=== Benchmark complete: " << total << " runs ===\n";

    // Для каждого алгоритма — средняя длина по всем инстансам
    std::cerr << "\nAlgorithm summary (avg total_length):\n";
    for (const auto& algo : algorithms) {
        double sum = 0.0;
        int cnt = 0;
        for (const auto& r : results) {
            if (r.algorithm == algo && r.valid) { sum += r.total_length; ++cnt; }
        }
        if (cnt > 0) {
            std::cerr << "  " << algo << ": " << (sum / cnt) << "\n";
        }
    }

    // Процент невалидных решений
    std::cerr << "\nInvalid solutions:\n";
    for (const auto& algo : algorithms) {
        int invalid = 0, total_a = 0;
        for (const auto& r : results) {
            if (r.algorithm == algo) { ++total_a; if (!r.valid) ++invalid; }
        }
        std::cerr << "  " << algo << ": " << invalid << "/" << total_a << "\n";
    }

    return 0;
}
