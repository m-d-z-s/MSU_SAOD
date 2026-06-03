/**
 * @file main.cpp
 * @brief Точка входа решателя задач маршрутизации транспорта (VRP).
 *
 * Использование:
 *   vrp_solver <файл_инстанса> [--algorithm <algo>] [--vehicles <n>]
 *                              [--capacity <c>] [--format <vrp|solomon>]
 *                              [--seed <s>] [--iters <n>] [--penalty <p>]
 *
 * Алгоритмы (--algorithm):
 *   nn       — Nearest Neighbor (жадный)
 *   cw       — Clarke-Wright Savings
 *   cw2opt   — CW + 2-opt
 *   sa       — CW + 2-opt + Simulated Annealing
 *   ts       — CW + Tabu Search
 *   hybrid   — CW + LS + SA (гибрид, по умолчанию)
 *
 * Примеры:
 *   vrp_solver data/A-n32-k5.vrp --algorithm hybrid
 *   vrp_solver data/C101.txt --format solomon --algorithm sa
 */

#include "core/distance.hpp"
#include "core/instance.hpp"
#include "core/solution.hpp"
#include "parsers/vrp_parser.hpp"
#include "parsers/solomon_parser.hpp"
#include "heuristics/nearest_neighbor.hpp"
#include "heuristics/clarke_wright.hpp"
#include "local_search/two_opt.hpp"
#include "local_search/or_opt.hpp"
#include "local_search/inter_route.hpp"
#include "metaheuristics/simulated_annealing.hpp"
#include "metaheuristics/tabu_search.hpp"
#include "hybrid/hybrid_solver.hpp"

#include <chrono>
#include <cstring>
#include <iostream>
#include <string>

using namespace vrp;

// ─── вспомогательные функции ──────────────────────────────────────────────────

/** @brief Выводит справку по использованию. */
static void print_usage(const char* prog) {
    std::cout
        << "Usage: " << prog
        << " <instance_file> [options]\n\n"
        << "Options:\n"
        << "  --algorithm <algo>   nn | cw | cw2opt | sa | ts | hybrid  (default: hybrid)\n"
        << "  --format <fmt>       vrp | solomon                         (default: vrp)\n"
        << "  --seed <n>           Random seed for SA/hybrid             (default: 42)\n"
        << "  --iters <n>          Max SA iterations without improvement (default: 50000)\n"
        << "  --penalty <p>        Vehicle penalty weight                (default: 10.0)\n"
        << "  --t-initial <t>      SA initial temperature                (default: 100.0)\n"
        << "  --alpha <a>          SA cooling coefficient                (default: 0.995)\n"
        << "\nAlgorithms:\n"
        << "  nn      Nearest Neighbor greedy heuristic\n"
        << "  cw      Clarke-Wright Savings (parallel)\n"
        << "  cw2opt  Clarke-Wright + 2-opt local search\n"
        << "  sa      CW + 2-opt + Simulated Annealing\n"
        << "  ts      CW + Tabu Search\n"
        << "  hybrid  CW + 2-opt + Or-opt + Relocate/Swap + SA  [default]\n";
}

/** @brief Выводит статистику решения. */
static void print_solution_stats(const Solution& sol,
                                 const DistanceMatrix& dist,
                                 const Instance& inst,
                                 double elapsed_ms,
                                 const std::string& algo_name) {
    std::string err = sol.validate(inst);
    bool valid = err.empty();

    std::cout << "\n=== " << inst.name << " | " << algo_name << " ===\n";
    std::cout << "Clients      : " << inst.num_clients() << "\n";
    std::cout << "Capacity     : " << inst.capacity << "\n";
    std::cout << "Routes used  : " << sol.num_routes() << "\n";
    std::cout << "Total length : " << sol.total_length(dist) << "\n";
    std::cout << "Valid        : " << (valid ? "YES" : "NO — " + err) << "\n";
    std::cout << "Time (ms)    : " << elapsed_ms << "\n";

    // Вывод маршрутов
    std::cout << "\nRoutes:\n";
    int ri = 1;
    for (const auto& r : sol.routes) {
        if (r.empty()) continue;
        std::cout << "  Route " << ri++ << " [demand="
                  << r.demand(inst) << "/" << inst.capacity
                  << ", len=" << r.length(dist) << "]: 0";
        for (int c : r.clients) std::cout << " -> " << c;
        std::cout << " -> 0\n";
    }
}

// ─── main ─────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    // ── разбор аргументов ───────────────────────────────────────────────────
    std::string filepath  = argv[1];
    std::string algorithm = "hybrid";
    std::string format    = "vrp";
    unsigned    seed      = 42;
    int         max_iter  = 50000;
    double      penalty   = 10.0;
    double      t_initial = 100.0;
    double      alpha     = 0.995;

    for (int i = 2; i < argc; ++i) {
        if (std::strcmp(argv[i], "--algorithm") == 0 && i + 1 < argc)
            algorithm = argv[++i];
        else if (std::strcmp(argv[i], "--format") == 0 && i + 1 < argc)
            format = argv[++i];
        else if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc)
            seed = static_cast<unsigned>(std::stoul(argv[++i]));
        else if (std::strcmp(argv[i], "--iters") == 0 && i + 1 < argc)
            max_iter = std::stoi(argv[++i]);
        else if (std::strcmp(argv[i], "--penalty") == 0 && i + 1 < argc)
            penalty = std::stod(argv[++i]);
        else if (std::strcmp(argv[i], "--t-initial") == 0 && i + 1 < argc)
            t_initial = std::stod(argv[++i]);
        else if (std::strcmp(argv[i], "--alpha") == 0 && i + 1 < argc)
            alpha = std::stod(argv[++i]);
        else if (std::strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown option: " << argv[i] << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    // ── загрузка инстанса ───────────────────────────────────────────────────
    Instance inst;
    try {
        if (format == "solomon") {
            inst = SolomonParser::parse(filepath);
        } else {
            inst = VrpParser::parse(filepath);
        }
    } catch (const std::exception& ex) {
        std::cerr << "Error loading instance: " << ex.what() << "\n";
        return 2;
    }

    if (inst.clients.empty()) {
        std::cerr << "Instance has no clients.\n";
        return 2;
    }

    // ── предвычисление матрицы расстояний ───────────────────────────────────
    DistanceMatrix dist(inst);

    // ── параметры алгоритмов ─────────────────────────────────────────────────
    SAParams sa_params;
    sa_params.seed            = seed;
    sa_params.max_iter        = max_iter;
    sa_params.vehicle_penalty = penalty;
    sa_params.t_initial       = t_initial;
    sa_params.alpha           = alpha;

    TSParams ts_params;
    ts_params.vehicle_penalty = penalty;

    HybridParams hybrid_params;
    hybrid_params.sa = sa_params;

    // ── запуск алгоритма ─────────────────────────────────────────────────────
    auto t_start = std::chrono::steady_clock::now();
    Solution sol;

    if (algorithm == "nn") {
        sol = NearestNeighbor::solve(inst, dist);
    } else if (algorithm == "cw") {
        sol = ClarkeWright::solve(inst, dist);
    } else if (algorithm == "cw2opt") {
        sol = ClarkeWright::solve(inst, dist);
        TwoOpt::improve(sol, inst, dist);
    } else if (algorithm == "sa") {
        sol = ClarkeWright::solve(inst, dist);
        TwoOpt::improve(sol, inst, dist);
        SimulatedAnnealing::optimize(sol, inst, dist, sa_params);
    } else if (algorithm == "ts") {
        sol = ClarkeWright::solve(inst, dist);
        TabuSearch::optimize(sol, inst, dist, ts_params);
    } else if (algorithm == "hybrid") {
        sol = HybridSolver::solve(inst, dist, hybrid_params);
    } else {
        std::cerr << "Unknown algorithm: " << algorithm << "\n";
        print_usage(argv[0]);
        return 1;
    }

    auto t_end = std::chrono::steady_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();

    // ── вывод результатов ────────────────────────────────────────────────────
    print_solution_stats(sol, dist, inst, elapsed_ms, algorithm);

    return 0;
}
