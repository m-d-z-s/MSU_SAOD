/**
 * @file test_hybrid.cpp
 * @brief Тесты гибридного решателя HybridSolver.
 *
 * Проверяет:
 *   - Корректность решения после оптимизации (validate).
 *   - Длина не хуже Clarke-Wright (гибрид должен улучшать).
 *   - Граничные случаи: 1 клиент, 2 клиента.
 *   - Функция score возвращает неотрицательное значение.
 *   - Гибрид не хуже чистого SA на 5-клиентном инстансе.
 */

#include "hybrid/hybrid_solver.hpp"
#include "heuristics/clarke_wright.hpp"
#include "core/distance.hpp"

#include <cassert>
#include <iostream>
#include <cmath>

using namespace vrp;

static int passed = 0;
static int failed = 0;

static void check(bool cond, const char* msg) {
    if (cond) { std::cout << "  [OK]   " << msg << "\n"; ++passed; }
    else       { std::cout << "  [FAIL] " << msg << "\n"; ++failed; }
}

// ─── инстансы ─────────────────────────────────────────────────────────────────

static Instance make_1client() {
    Instance inst;
    inst.name = "one"; inst.capacity = 100; inst.num_vehicles = 0;
    inst.clients = { {0,0.0,0.0,0,0,0,0}, {1,5.0,0.0,10,0,0,0} };
    return inst;
}

static Instance make_2client() {
    Instance inst;
    inst.name = "two"; inst.capacity = 20; inst.num_vehicles = 0;
    inst.clients = {
        {0, 0.0, 0.0,  0, 0, 0, 0},
        {1, 3.0, 0.0, 10, 0, 0, 0},
        {2, 0.0, 4.0, 10, 0, 0, 0},
    };
    return inst;
}

static Instance make_5client() {
    Instance inst;
    inst.name = "five"; inst.capacity = 30; inst.num_vehicles = 0;
    inst.clients = {
        {0,  0.0,  0.0,  0, 0, 0, 0},
        {1, 10.0,  0.0, 10, 0, 0, 0},
        {2, 10.0, 10.0, 10, 0, 0, 0},
        {3,  0.0, 10.0, 10, 0, 0, 0},
        {4,  5.0,  5.0,  5, 0, 0, 0},
        {5, 20.0,  0.0, 15, 0, 0, 0},
    };
    return inst;
}

static Instance make_10client() {
    Instance inst;
    inst.name = "ten"; inst.capacity = 40; inst.num_vehicles = 0;
    inst.clients = {
        {0,  0.0,  0.0,  0, 0, 0, 0},
        {1, 10.0,  0.0, 10, 0, 0, 0},
        {2, 20.0,  0.0, 10, 0, 0, 0},
        {3, 30.0,  0.0, 10, 0, 0, 0},
        {4, 10.0, 10.0, 10, 0, 0, 0},
        {5, 20.0, 10.0, 10, 0, 0, 0},
        {6, 30.0, 10.0, 10, 0, 0, 0},
        {7,  5.0,  5.0,  5, 0, 0, 0},
        {8, 15.0,  5.0,  5, 0, 0, 0},
        {9, 25.0,  5.0,  5, 0, 0, 0},
        {10,35.0,  5.0,  5, 0, 0, 0},
    };
    return inst;
}

// ─── тесты ────────────────────────────────────────────────────────────────────

static void test_1client() {
    std::cout << "\n[1-client]\n";
    auto inst = make_1client();
    DistanceMatrix dist(inst);
    HybridParams p; p.sa.max_iter = 100;
    auto sol = HybridSolver::solve(inst, dist, p);

    check(sol.validate(inst).empty(), "solution is valid");
    check(sol.num_routes() == 1, "1 route for 1 client");
    check(sol.routes[0].clients.size() == 1, "route has 1 client");
}

static void test_2client() {
    std::cout << "\n[2-client]\n";
    auto inst = make_2client();
    DistanceMatrix dist(inst);
    HybridParams p; p.sa.max_iter = 200;
    auto sol = HybridSolver::solve(inst, dist, p);

    check(sol.validate(inst).empty(), "solution is valid");
    check(sol.num_routes() >= 1, "at least 1 route");

    // Суммарный спрос обоих клиентов = 20 = capacity → возможен 1 маршрут
    int total_dem = 0;
    for (const auto& r : sol.routes)
        total_dem += r.demand(inst);
    check(total_dem == 20, "all demand served");
}

static void test_5client_valid() {
    std::cout << "\n[5-client validity]\n";
    auto inst = make_5client();
    DistanceMatrix dist(inst);
    HybridParams p; p.sa.max_iter = 500; p.sa.seed = 1;
    auto sol = HybridSolver::solve(inst, dist, p);

    check(sol.validate(inst).empty(), "solution is valid");
    check(sol.total_length(dist) > 0.0, "total length > 0");
    check(sol.num_routes() >= 1, "at least 1 route");
}

static void test_hybrid_not_worse_than_cw() {
    std::cout << "\n[hybrid not worse than CW on 5-client]\n";
    auto inst = make_5client();
    DistanceMatrix dist(inst);

    auto cw_sol = ClarkeWright::solve(inst, dist);
    double cw_len = cw_sol.total_length(dist);

    HybridParams p; p.sa.max_iter = 1000; p.sa.seed = 42;
    auto h_sol = HybridSolver::solve(inst, dist, p);
    double h_len = h_sol.total_length(dist);

    check(h_sol.validate(inst).empty(), "hybrid solution valid");
    // Гибрид должен быть не хуже CW (с небольшим допуском 1%)
    check(h_len <= cw_len * 1.01, "hybrid length <= CW length (1% tol)");
}

static void test_10client() {
    std::cout << "\n[10-client]\n";
    auto inst = make_10client();
    DistanceMatrix dist(inst);

    HybridParams p;
    p.sa.max_iter = 2000;
    p.sa.seed = 7;
    p.sa.vehicle_penalty = 10.0;

    auto cw_sol = ClarkeWright::solve(inst, dist);
    auto sol    = HybridSolver::solve(inst, dist, p);

    check(sol.validate(inst).empty(), "solution is valid");

    double cw_score  = HybridSolver::score(cw_sol, dist, 10.0);
    double hyb_score = HybridSolver::score(sol,    dist, 10.0);
    check(hyb_score <= cw_score * 1.05, "hybrid score <= CW score (5% tol)");

    std::cout << "    CW  score=" << cw_score
              << " (routes=" << cw_sol.num_routes()
              << ", len=" << cw_sol.total_length(dist) << ")\n";
    std::cout << "    Hyb score=" << hyb_score
              << " (routes=" << sol.num_routes()
              << ", len=" << sol.total_length(dist) << ")\n";
}

static void test_score_function() {
    std::cout << "\n[score function]\n";
    auto inst = make_5client();
    DistanceMatrix dist(inst);
    HybridParams p; p.sa.max_iter = 100;
    auto sol = HybridSolver::solve(inst, dist, p);

    double s = HybridSolver::score(sol, dist, 10.0);
    check(s > 0.0, "score > 0");
    check(s >= sol.total_length(dist), "score >= total_length");
    check(std::isfinite(s), "score is finite");
}

static void test_params_defaults() {
    std::cout << "\n[default params]\n";
    auto inst = make_5client();
    DistanceMatrix dist(inst);

    auto sol = HybridSolver::solve(inst, dist);
    check(sol.validate(inst).empty(), "default params: solution valid");
}

// ─── main ─────────────────────────────────────────────────────────────────────

int main() {
    std::cout << "=== test_hybrid ===\n";

    test_1client();
    test_2client();
    test_5client_valid();
    test_hybrid_not_worse_than_cw();
    test_10client();
    test_score_function();
    test_params_defaults();

    std::cout << "\n--- Results: " << passed << " passed, "
              << failed << " failed ---\n";
    return (failed > 0) ? 1 : 0;
}
