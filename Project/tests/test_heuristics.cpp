/**
 * @brief Тесты конструктивных эвристик: NearestNeighbor и ClarkeWright.
 *
 * Проверяет:
 *   - корректность решения (validate == "")
 *   - базовые случаи: 1 клиент, все клиенты в одном маршруте
 *   - Clarke-Wright даёт решение не хуже NN на простом примере
 */

#include "heuristics/nearest_neighbor.hpp"
#include "heuristics/clarke_wright.hpp"
#include "core/distance.hpp"

#include <cassert>
#include <iostream>
#include <cmath>

using namespace vrp;

// ─── helpers ─────────────────────────────────────────────────────────────────

static int passed = 0;
static int failed = 0;

static void check(bool cond, const char* msg) {
    if (cond) {
        std::cout << "  [OK] " << msg << "\n";
        ++passed;
    } else {
        std::cout << "  [FAIL] " << msg << "\n";
        ++failed;
    }
}

/** Создаёт инстанс с 5 клиентами, вместимость 30. */
static Instance make_5client() {
    Instance inst;
    inst.name      = "five";
    inst.capacity  = 30;
    inst.num_vehicles = 0;
    inst.clients = {
        {0,  0.0,  0.0,  0, 0, 0, 0},  // депо
        {1, 10.0,  0.0, 10, 0, 0, 0},
        {2, 10.0, 10.0, 10, 0, 0, 0},
        {3,  0.0, 10.0, 10, 0, 0, 0},
        {4,  5.0,  5.0,  5, 0, 0, 0},
        {5, 20.0,  0.0, 15, 0, 0, 0},
    };
    return inst;
}

/** Создаёт инстанс: 1 клиент. */
static Instance make_1client() {
    Instance inst;
    inst.name     = "one";
    inst.capacity = 100;
    inst.num_vehicles = 0;
    inst.clients = {
        {0, 0.0, 0.0,  0, 0, 0, 0},
        {1, 5.0, 0.0, 10, 0, 0, 0},
    };
    return inst;
}

/** Создаёт инстанс, где все клиенты помещаются в один маршрут. */
static Instance make_all_one_route() {
    Instance inst;
    inst.name     = "one_route";
    inst.capacity = 100;
    inst.num_vehicles = 0;
    inst.clients = {
        {0, 0.0, 0.0,  0, 0, 0, 0},
        {1, 1.0, 0.0, 10, 0, 0, 0},
        {2, 2.0, 0.0, 10, 0, 0, 0},
        {3, 3.0, 0.0, 10, 0, 0, 0},
    };
    return inst;
}

// ─── NearestNeighbor tests ───────────────────────────────────────────────────

static void test_nn_5client() {
    std::cout << "\n[NearestNeighbor] 5 clients\n";
    Instance inst = make_5client();
    DistanceMatrix d(inst);
    Solution sol = NearestNeighbor::solve(inst, d);

    auto err = sol.validate(inst);
    check(err.empty(), ("valid solution: " + err).c_str());
    check(sol.num_routes() >= 1, "at least one route");
}

static void test_nn_single_client() {
    std::cout << "\n[NearestNeighbor] single client\n";
    Instance inst = make_1client();
    DistanceMatrix d(inst);
    Solution sol = NearestNeighbor::solve(inst, d);

    auto err = sol.validate(inst);
    check(err.empty(), ("valid: " + err).c_str());
    check(sol.num_routes() == 1, "exactly 1 route");
    check(sol.routes[0].clients.size() == 1, "route has 1 client");
}

static void test_nn_all_one_route() {
    std::cout << "\n[NearestNeighbor] all clients fit one route\n";
    Instance inst = make_all_one_route();
    DistanceMatrix d(inst);
    Solution sol = NearestNeighbor::solve(inst, d);

    auto err = sol.validate(inst);
    check(err.empty(), ("valid: " + err).c_str());
    // NN должна уложить всех в один маршрут (суммарный спрос 30 <= 100).
    check(sol.num_routes() == 1, "all in 1 route");
}

// ─── ClarkeWright tests ──────────────────────────────────────────────────────

static void test_cw_5client() {
    std::cout << "\n[ClarkeWright] 5 clients\n";
    Instance inst = make_5client();
    DistanceMatrix d(inst);
    Solution sol = ClarkeWright::solve(inst, d);

    auto err = sol.validate(inst);
    check(err.empty(), ("valid solution: " + err).c_str());
    check(sol.num_routes() >= 1, "at least one route");
}

static void test_cw_single_client() {
    std::cout << "\n[ClarkeWright] single client\n";
    Instance inst = make_1client();
    DistanceMatrix d(inst);
    Solution sol = ClarkeWright::solve(inst, d);

    auto err = sol.validate(inst);
    check(err.empty(), ("valid: " + err).c_str());
    check(sol.num_routes() == 1, "exactly 1 route");
}

static void test_cw_all_one_route() {
    std::cout << "\n[ClarkeWright] all clients fit one route\n";
    Instance inst = make_all_one_route();
    DistanceMatrix d(inst);
    Solution sol = ClarkeWright::solve(inst, d);

    auto err = sol.validate(inst);
    check(err.empty(), ("valid: " + err).c_str());
    // CW должна объединить всех в один маршрут.
    check(sol.num_routes() == 1, "all merged into 1 route");
}

static void test_cw_not_worse_than_nn() {
    std::cout << "\n[ClarkeWright vs NearestNeighbor] quality comparison\n";
    Instance inst = make_5client();
    DistanceMatrix d(inst);

    Solution nn_sol = NearestNeighbor::solve(inst, d);
    Solution cw_sol = ClarkeWright::solve(inst, d);

    double nn_len = nn_sol.total_length(d);
    double cw_len = cw_sol.total_length(d);

    std::cout << "  NN length=" << nn_len << "  CW length=" << cw_len << "\n";
    // CW по построению должна давать экономию — длина <= NN.
    check(cw_len <= nn_len + 1e-6, "CW length <= NN length");
}

// ─── main ─────────────────────────────────────────────────────────────────────

int main() {
    std::cout << "=== test_heuristics ===\n";

    test_nn_5client();
    test_nn_single_client();
    test_nn_all_one_route();

    test_cw_5client();
    test_cw_single_client();
    test_cw_all_one_route();

    test_cw_not_worse_than_nn();

    std::cout << "\n--- Results: " << passed << " passed, " << failed << " failed ---\n";
    return failed == 0 ? 0 : 1;
}
