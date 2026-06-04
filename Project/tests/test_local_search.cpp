/**
 * @brief Тесты модуля local_search: TwoOpt, OrOpt, InterRoute.
 *
 * Проверяет:
 *   - Корректность решения после применения каждого оператора (validate).
 *   - Длина не возрастает после улучшения.
 *   - Граничные случаи: 1 клиент, 2 клиента, все клиенты в одном маршруте.
 *   - Inter-route: Relocate уменьшает число маршрутов, если возможно.
 *   - Цепочка CW -> 2-opt -> Or-opt -> Relocate даёт валидное решение.
 */

#include "local_search/two_opt.hpp"
#include "local_search/or_opt.hpp"
#include "local_search/inter_route.hpp"
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
        std::cout << "  [OK]   " << msg << "\n";
        ++passed;
    } else {
        std::cout << "  [FAIL] " << msg << "\n";
        ++failed;
    }
}

/** Инстанс: 5 клиентов, вместимость 30 (нужно >= 2 машины). */
static Instance make_5client() {
    Instance inst;
    inst.name         = "five";
    inst.capacity     = 30;
    inst.num_vehicles = 0;
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

/** Инстанс: 1 клиент. */
static Instance make_1client() {
    Instance inst;
    inst.name         = "one";
    inst.capacity     = 100;
    inst.num_vehicles = 0;
    inst.clients = {
        {0, 0.0, 0.0,  0, 0, 0, 0},
        {1, 5.0, 0.0, 10, 0, 0, 0},
    };
    return inst;
}

/** Инстанс: все клиенты влезают в один маршрут. */
static Instance make_one_route() {
    Instance inst;
    inst.name         = "one_route";
    inst.capacity     = 100;
    inst.num_vehicles = 0;
    inst.clients = {
        {0, 0.0, 0.0,  0, 0, 0, 0},
        {1, 1.0, 0.0, 10, 0, 0, 0},
        {2, 3.0, 0.0, 10, 0, 0, 0},
        {3, 2.0, 0.0, 10, 0, 0, 0}, // нарочно «неправильный» порядок
    };
    return inst;
}

/**
 * @brief Инстанс с явно плохим начальным решением для Relocate:
 *   два маршрута, один из которых можно объединить с другим.
 */
static Solution make_split_solution(const Instance& /*inst*/) {
    // Клиенты 1,2,3: спрос по 10, вместимость 100 -> все в один маршрут.
    // Принудительно создаём 2 маршрута.
    Solution sol;
    Route r1, r2;
    r1.clients = {1, 2};
    r2.clients = {3};
    sol.routes.push_back(r1);
    sol.routes.push_back(r2);
    return sol;
}

// ─── 2-opt tests ─────────────────────────────────────────────────────────────

static void test_two_opt_1client() {
    std::cout << "\n[2-opt] single client\n";
    Instance inst = make_1client();
    DistanceMatrix d(inst);
    Solution sol = ClarkeWright::solve(inst, d);
    double before = sol.total_length(d);

    double gain = TwoOpt::improve(sol, inst, d);

    check(sol.validate(inst).empty(), "valid after 2-opt");
    check(gain >= 0.0, "gain >= 0");
    check(sol.total_length(d) <= before + 1e-9, "length did not increase");
}

static void test_two_opt_5client() {
    std::cout << "\n[2-opt] 5 clients (CW start)\n";
    Instance inst = make_5client();
    DistanceMatrix d(inst);
    Solution sol = ClarkeWright::solve(inst, d);
    double before = sol.total_length(d);

    double gain = TwoOpt::improve(sol, inst, d);

    check(sol.validate(inst).empty(), "valid after 2-opt");
    check(gain >= 0.0, "gain >= 0");
    check(sol.total_length(d) <= before + 1e-9, "length did not increase");
}

static void test_two_opt_improves_bad_route() {
    std::cout << "\n[2-opt] explicitly bad route: 0-3-1-2-0\n";
    // Маршрут 3->1->2 хуже, чем 1->2->3 или 1->3->2.
    Instance inst = make_one_route();
    DistanceMatrix d(inst);

    // Создаём заведомо плохой маршрут вручную.
    Solution sol;
    Route r;
    r.clients = {3, 1, 2}; // порядок: 0->3->1->2->0
    sol.routes.push_back(r);

    double before = sol.total_length(d);
    TwoOpt::improve(sol, inst, d);
    double after = sol.total_length(d);

    check(sol.validate(inst).empty(), "valid after 2-opt");
    check(after <= before + 1e-9, "length did not increase");
    // Оптимальный порядок для точек на оси X: 1,2,3 -> длина 3+3=6.
    // Плохой порядок 3,1,2: 0->3(dist=2), 3->1(dist=2), 1->2(dist=1), 2->0(dist=3) = 8.
    // После 2-opt должно стать 6 (или 7 для 1,3,2).
    check(after < before - 1e-9, "length improved on bad route");
}

static void test_two_opt_already_optimal() {
    std::cout << "\n[2-opt] already optimal (collinear)\n";
    Instance inst = make_one_route();
    DistanceMatrix d(inst);

    // Оптимальный маршрут: 1->2->3.
    Solution sol;
    Route r;
    r.clients = {1, 2, 3};
    sol.routes.push_back(r);

    double before = sol.total_length(d);
    double gain   = TwoOpt::improve(sol, inst, d);

    check(sol.validate(inst).empty(), "valid");
    check(std::fabs(gain) < 1e-9, "no gain on optimal route");
    check(std::fabs(sol.total_length(d) - before) < 1e-9, "length unchanged");
}

// ─── Or-opt tests ─────────────────────────────────────────────────────────────

static void test_or_opt_1client() {
    std::cout << "\n[Or-opt] single client\n";
    Instance inst = make_1client();
    DistanceMatrix d(inst);
    Solution sol = ClarkeWright::solve(inst, d);
    double before = sol.total_length(d);

    double gain = OrOpt::improve(sol, inst, d);

    check(sol.validate(inst).empty(), "valid after Or-opt");
    check(gain >= 0.0, "gain >= 0");
    check(sol.total_length(d) <= before + 1e-9, "length did not increase");
}

static void test_or_opt_5client() {
    std::cout << "\n[Or-opt] 5 clients (CW start)\n";
    Instance inst = make_5client();
    DistanceMatrix d(inst);
    Solution sol = ClarkeWright::solve(inst, d);
    double before = sol.total_length(d);

    double gain = OrOpt::improve(sol, inst, d);

    check(sol.validate(inst).empty(), "valid after Or-opt");
    check(gain >= 0.0, "gain >= 0");
    check(sol.total_length(d) <= before + 1e-9, "length did not increase");
}

static void test_or_opt_k1_improves() {
    std::cout << "\n[Or-opt k=1] relocate single client inside route\n";
    Instance inst = make_one_route();
    DistanceMatrix d(inst);

    // Плохой порядок: 3,1,2
    Solution sol;
    Route r;
    r.clients = {3, 1, 2};
    sol.routes.push_back(r);

    double before = sol.total_length(d);
    OrOpt::improve(sol, inst, d);
    double after = sol.total_length(d);

    check(sol.validate(inst).empty(), "valid after Or-opt k=1");
    check(after <= before + 1e-9, "length did not increase");
}

// ─── Inter-route tests ────────────────────────────────────────────────────────

static void test_relocate_merges_routes() {
    std::cout << "\n[Relocate] merges routes when possible\n";
    Instance inst = make_one_route(); // вместимость 100, все спросы по 10
    DistanceMatrix d(inst);

    // 2 маршрута: {1,2} и {3}. Вместимость 100 — можно перенести 3 в первый.
    Solution sol = make_split_solution(inst);
    check(sol.validate(inst).empty(), "initial solution valid");

    int routes_before = sol.num_routes();
    double before     = sol.total_length(d);

    double gain = InterRoute::relocate(sol, inst, d);

    check(sol.validate(inst).empty(), "valid after relocate");
    check(gain >= 0.0, "gain >= 0");
    check(sol.total_length(d) <= before + 1e-9, "length did not increase");
    check(sol.num_routes() <= routes_before, "routes count did not grow");
    // Конкретно: должен объединиться в 1 маршрут.
    check(sol.num_routes() == 1, "merged into 1 route");
}

static void test_swap_valid() {
    std::cout << "\n[Swap] valid after swap on 5-client instance\n";
    Instance inst = make_5client();
    DistanceMatrix d(inst);
    Solution sol = ClarkeWright::solve(inst, d);
    double before = sol.total_length(d);

    double gain = InterRoute::swap(sol, inst, d);

    check(sol.validate(inst).empty(), "valid after swap");
    check(gain >= 0.0, "gain >= 0");
    check(sol.total_length(d) <= before + 1e-9, "length did not increase");
}

static void test_inter_route_single_route() {
    std::cout << "\n[InterRoute] single route — nothing to do\n";
    Instance inst = make_one_route();
    DistanceMatrix d(inst);
    Solution sol;
    Route r;
    r.clients = {1, 2, 3};
    sol.routes.push_back(r);

    double before = sol.total_length(d);
    double gain   = InterRoute::improve(sol, inst, d);

    check(sol.validate(inst).empty(), "valid");
    check(gain >= 0.0, "gain >= 0");
    check(std::fabs(sol.total_length(d) - before) < 1e-9, "length unchanged (1 route)");
}

// ─── Full pipeline test ───────────────────────────────────────────────────────

static void test_full_pipeline() {
    std::cout << "\n[Pipeline] CW -> 2-opt -> Or-opt -> Relocate+Swap\n";
    Instance inst = make_5client();
    DistanceMatrix d(inst);

    Solution sol = ClarkeWright::solve(inst, d);
    double len0 = sol.total_length(d);

    TwoOpt::improve(sol, inst, d);
    double len1 = sol.total_length(d);

    OrOpt::improve(sol, inst, d);
    double len2 = sol.total_length(d);

    InterRoute::improve(sol, inst, d);
    double len3 = sol.total_length(d);

    check(sol.validate(inst).empty(), "pipeline: valid solution");
    check(len1 <= len0 + 1e-9, "2-opt did not increase");
    check(len2 <= len1 + 1e-9, "Or-opt did not increase");
    check(len3 <= len2 + 1e-9, "InterRoute did not increase");

    std::cout << "  CW=" << len0 << " 2opt=" << len1
              << " Oropt=" << len2 << " Inter=" << len3 << "\n";
}

// ─── main ─────────────────────────────────────────────────────────────────────

int main() {
    std::cout << "=== test_local_search ===\n";

    test_two_opt_1client();
    test_two_opt_5client();
    test_two_opt_improves_bad_route();
    test_two_opt_already_optimal();

    test_or_opt_1client();
    test_or_opt_5client();
    test_or_opt_k1_improves();

    test_relocate_merges_routes();
    test_swap_valid();
    test_inter_route_single_route();

    test_full_pipeline();

    std::cout << "\n--- Results: " << passed << " passed, " << failed << " failed ---\n";
    return failed == 0 ? 0 : 1;
}
