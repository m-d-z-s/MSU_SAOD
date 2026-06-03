/**
 * @brief Тесты метаэвристик: SimulatedAnnealing и TabuSearch.
 *
 * Проверяет:
 *   - Корректность решения после оптимизации (validate).
 *   - Длина не возрастает по сравнению со стартовым решением (CW).
 *   - Граничные случаи: 1 клиент, 2 клиента.
 *   - SA и TS не хуже CW на 5-клиентном инстансе.
 *   - Повторный запуск с тем же seed воспроизводим (SA).
 */

#include "metaheuristics/simulated_annealing.hpp"
#include "metaheuristics/tabu_search.hpp"
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

/** 8 клиентов, умеренная вместимость — есть пространство для улучшений. */
static Instance make_8client() {
    Instance inst;
    inst.name = "eight"; inst.capacity = 40; inst.num_vehicles = 0;
    inst.clients = {
        {0,  0.0,  0.0,  0, 0, 0, 0},
        {1, 10.0,  2.0, 10, 0, 0, 0},
        {2, 15.0,  5.0, 10, 0, 0, 0},
        {3, 12.0, 12.0, 10, 0, 0, 0},
        {4,  5.0, 15.0, 10, 0, 0, 0},
        {5,  0.0,  8.0, 10, 0, 0, 0},
        {6,  8.0,  0.0, 10, 0, 0, 0},
        {7, 20.0,  0.0, 10, 0, 0, 0},
        {8,  3.0, 20.0, 10, 0, 0, 0},
    };
    return inst;
}

// ─── SA tests ─────────────────────────────────────────────────────────────────

static void test_sa_1client() {
    std::cout << "\n[SA] single client\n";
    Instance inst = make_1client();
    DistanceMatrix d(inst);
    Solution sol = ClarkeWright::solve(inst, d);
    double before = sol.total_length(d);

    SimulatedAnnealing::optimize(sol, inst, d);

    check(sol.validate(inst).empty(), "valid after SA");
    check(sol.total_length(d) <= before + 1e-9, "length did not increase");
}

static void test_sa_5client() {
    std::cout << "\n[SA] 5 clients\n";
    Instance inst = make_5client();
    DistanceMatrix d(inst);
    Solution sol = ClarkeWright::solve(inst, d);
    double before = sol.total_length(d);

    SimulatedAnnealing::optimize(sol, inst, d);

    check(sol.validate(inst).empty(), "valid after SA");
    check(sol.total_length(d) <= before + 1e-9, "SA not worse than CW");
    std::cout << "  CW=" << before << "  SA=" << sol.total_length(d) << "\n";
}

static void test_sa_8client() {
    std::cout << "\n[SA] 8 clients\n";
    Instance inst = make_8client();
    DistanceMatrix d(inst);
    Solution sol = ClarkeWright::solve(inst, d);
    double before = sol.total_length(d);

    SAParams params;
    params.max_iter = 20000;
    SimulatedAnnealing::optimize(sol, inst, d, params);

    check(sol.validate(inst).empty(), "valid after SA");
    check(sol.total_length(d) <= before + 1e-9, "SA not worse than CW");
    std::cout << "  CW=" << before << "  SA=" << sol.total_length(d) << "\n";
}

static void test_sa_reproducible() {
    std::cout << "\n[SA] reproducible with same seed\n";
    Instance inst = make_8client();
    DistanceMatrix d(inst);

    SAParams params;
    params.seed = 123;
    params.max_iter = 5000;

    Solution sol1 = ClarkeWright::solve(inst, d);
    SimulatedAnnealing::optimize(sol1, inst, d, params);

    Solution sol2 = ClarkeWright::solve(inst, d);
    SimulatedAnnealing::optimize(sol2, inst, d, params);

    check(sol1.validate(inst).empty(), "run1 valid");
    check(sol2.validate(inst).empty(), "run2 valid");
    check(std::fabs(sol1.total_length(d) - sol2.total_length(d)) < 1e-9,
          "same seed => same result");
}

static void test_sa_different_seeds() {
    std::cout << "\n[SA] different seeds give valid results\n";
    Instance inst = make_8client();
    DistanceMatrix d(inst);
    Solution cw = ClarkeWright::solve(inst, d);
    double cw_len = cw.total_length(d);

    for (unsigned seed : {1u, 2u, 99u}) {
        SAParams p; p.seed = seed; p.max_iter = 5000;
        Solution sol = ClarkeWright::solve(inst, d);
        SimulatedAnnealing::optimize(sol, inst, d, p);
        check(sol.validate(inst).empty(), ("valid seed=" + std::to_string(seed)).c_str());
        check(sol.total_length(d) <= cw_len + 1e-9,
              ("not worse than CW seed=" + std::to_string(seed)).c_str());
    }
}

// ─── TS tests ─────────────────────────────────────────────────────────────────

static void test_ts_1client() {
    std::cout << "\n[TS] single client\n";
    Instance inst = make_1client();
    DistanceMatrix d(inst);
    Solution sol = ClarkeWright::solve(inst, d);
    double before = sol.total_length(d);

    TabuSearch::optimize(sol, inst, d);

    check(sol.validate(inst).empty(), "valid after TS");
    check(sol.total_length(d) <= before + 1e-9, "length did not increase");
}

static void test_ts_5client() {
    std::cout << "\n[TS] 5 clients\n";
    Instance inst = make_5client();
    DistanceMatrix d(inst);
    Solution sol = ClarkeWright::solve(inst, d);
    double before = sol.total_length(d);

    TabuSearch::optimize(sol, inst, d);

    check(sol.validate(inst).empty(), "valid after TS");
    check(sol.total_length(d) <= before + 1e-9, "TS not worse than CW");
    std::cout << "  CW=" << before << "  TS=" << sol.total_length(d) << "\n";
}

static void test_ts_8client() {
    std::cout << "\n[TS] 8 clients\n";
    Instance inst = make_8client();
    DistanceMatrix d(inst);
    Solution sol = ClarkeWright::solve(inst, d);
    double before = sol.total_length(d);

    TSParams params;
    params.max_iter = 300;
    TabuSearch::optimize(sol, inst, d, params);

    check(sol.validate(inst).empty(), "valid after TS");
    check(sol.total_length(d) <= before + 1e-9, "TS not worse than CW");
    std::cout << "  CW=" << before << "  TS=" << sol.total_length(d) << "\n";
}

static void test_ts_tenure() {
    std::cout << "\n[TS] tabu tenure variation\n";
    Instance inst = make_8client();
    DistanceMatrix d(inst);
    double cw_len = ClarkeWright::solve(inst, d).total_length(d);

    for (int tenure : {5, 10, 20}) {
        Solution sol = ClarkeWright::solve(inst, d);
        TSParams p; p.tabu_tenure = tenure; p.max_iter = 200;
        TabuSearch::optimize(sol, inst, d, p);
        check(sol.validate(inst).empty(),
              ("valid tenure=" + std::to_string(tenure)).c_str());
        check(sol.total_length(d) <= cw_len + 1e-9,
              ("not worse CW tenure=" + std::to_string(tenure)).c_str());
    }
}

// ─── SA vs TS comparison ──────────────────────────────────────────────────────

static void test_sa_vs_ts_quality() {
    std::cout << "\n[SA vs TS] quality on 8 clients\n";
    Instance inst = make_8client();
    DistanceMatrix d(inst);

    Solution sa_sol = ClarkeWright::solve(inst, d);
    SAParams sa_p; sa_p.max_iter = 20000; sa_p.seed = 42;
    SimulatedAnnealing::optimize(sa_sol, inst, d, sa_p);

    Solution ts_sol = ClarkeWright::solve(inst, d);
    TSParams ts_p; ts_p.max_iter = 300;
    TabuSearch::optimize(ts_sol, inst, d, ts_p);

    double cw_len = ClarkeWright::solve(inst, d).total_length(d);
    double sa_len = sa_sol.total_length(d);
    double ts_len = ts_sol.total_length(d);

    std::cout << "  CW=" << cw_len << "  SA=" << sa_len << "  TS=" << ts_len << "\n";

    check(sa_sol.validate(inst).empty(), "SA valid");
    check(ts_sol.validate(inst).empty(), "TS valid");
    check(sa_len <= cw_len + 1e-9, "SA <= CW");
    check(ts_len <= cw_len + 1e-9, "TS <= CW");
}

// ─── main ─────────────────────────────────────────────────────────────────────

int main() {
    std::cout << "=== test_metaheuristics ===\n";

    test_sa_1client();
    test_sa_5client();
    test_sa_8client();
    test_sa_reproducible();
    test_sa_different_seeds();

    test_ts_1client();
    test_ts_5client();
    test_ts_8client();
    test_ts_tenure();

    test_sa_vs_ts_quality();

    std::cout << "\n--- Results: " << passed << " passed, "
              << failed << " failed ---\n";
    return failed == 0 ? 0 : 1;
}
