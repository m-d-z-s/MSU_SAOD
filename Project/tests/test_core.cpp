#include "../src/core/instance.hpp"
#include "../src/core/distance.hpp"
#include "../src/core/solution.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

using namespace vrp;

// ─── helpers ─────────────────────────────────────────────────────────────────

static Instance make_simple() {
    // Депо в (0,0), три клиента: (3,0), (0,4), (1,1)
    // Спрос: 10, 15, 5. Вместимость: 20.
    Instance inst;
    inst.name     = "test";
    inst.capacity = 20;
    inst.num_vehicles = 0;
    inst.clients = {
        {0, 0.0, 0.0, 0,  0, 0, 0},  // депо
        {1, 3.0, 0.0, 10, 0, 0, 0},  // клиент 1
        {2, 0.0, 4.0, 15, 0, 0, 0},  // клиент 2
        {3, 1.0, 1.0, 5,  0, 0, 0},  // клиент 3
    };
    return inst;
}

// ─── distance matrix tests ───────────────────────────────────────────────────

static void test_dist_self_zero() {
    Instance inst = make_simple();
    DistanceMatrix d(inst);
    for (int i = 0; i < d.size(); ++i) {
        assert(d(i, i) == 0.0 && "distance to self must be 0");
    }
    std::cout << "[PASS] dist self-zero\n";
}

static void test_dist_symmetric() {
    Instance inst = make_simple();
    DistanceMatrix d(inst);
    for (int i = 0; i < d.size(); ++i) {
        for (int j = 0; j < d.size(); ++j) {
            assert(std::abs(d(i, j) - d(j, i)) < 1e-9 && "distance must be symmetric");
        }
    }
    std::cout << "[PASS] dist symmetric\n";
}

static void test_dist_known_value() {
    Instance inst = make_simple();
    DistanceMatrix d(inst);
    // dist(depot, client1) = sqrt(9+0) = 3
    assert(std::abs(d(0, 1) - 3.0) < 1e-9 && "dist(0,1) must be 3");
    // dist(depot, client2) = sqrt(0+16) = 4
    assert(std::abs(d(0, 2) - 4.0) < 1e-9 && "dist(0,2) must be 4");
    std::cout << "[PASS] dist known values\n";
}

// ─── solution tests ───────────────────────────────────────────────────────────

static void test_route_demand() {
    Instance inst = make_simple();
    Route r;
    r.clients = {1, 3};  // demand 10 + 5 = 15
    assert(r.demand(inst) == 15 && "route demand must be 15");
    std::cout << "[PASS] route demand\n";
}

static void test_route_length() {
    Instance inst = make_simple();
    DistanceMatrix d(inst);
    Route r;
    r.clients = {1};  // depot->1->depot = 3+3 = 6
    assert(std::abs(r.length(d) - 6.0) < 1e-9 && "single-client route length");
    std::cout << "[PASS] route length\n";
}

static void test_validate_ok() {
    Instance inst = make_simple();
    Solution sol;
    sol.routes.resize(2);
    sol.routes[0].clients = {1, 3}; // demand 15 <= 20
    sol.routes[1].clients = {2};    // demand 15 <= 20
    auto err = sol.validate(inst);
    assert(err.empty() && ("validate failed: " + err).c_str());
    std::cout << "[PASS] validate ok\n";
}

static void test_validate_capacity() {
    Instance inst = make_simple();
    Solution sol;
    sol.routes.resize(1);
    sol.routes[0].clients = {1, 2}; // demand 10+15=25 > 20
    auto err = sol.validate(inst);
    assert(!err.empty() && "should detect capacity violation");
    std::cout << "[PASS] validate capacity violation\n";
}

static void test_validate_duplicate() {
    Instance inst = make_simple();
    Solution sol;
    sol.routes.resize(2);
    sol.routes[0].clients = {1, 2};
    sol.routes[1].clients = {1, 3}; // клиент 1 дважды
    auto err = sol.validate(inst);
    assert(!err.empty() && "should detect duplicate client");
    std::cout << "[PASS] validate duplicate client\n";
}

static void test_validate_missing() {
    Instance inst = make_simple();
    Solution sol;
    sol.routes.resize(1);
    sol.routes[0].clients = {1}; // клиент 2 и 3 не посещены
    auto err = sol.validate(inst);
    assert(!err.empty() && "should detect missing clients");
    std::cout << "[PASS] validate missing client\n";
}

static void test_single_client() {
    // Граничный случай: один клиент
    Instance inst;
    inst.name     = "single";
    inst.capacity = 100;
    inst.num_vehicles = 0;
    inst.clients = {
        {0, 0.0, 0.0, 0, 0, 0, 0},
        {1, 5.0, 0.0, 10, 0, 0, 0},
    };
    DistanceMatrix d(inst);
    assert(std::abs(d(0, 1) - 5.0) < 1e-9);

    Solution sol;
    sol.routes.resize(1);
    sol.routes[0].clients = {1};
    assert(sol.validate(inst).empty() && "single client should be valid");
    std::cout << "[PASS] single client\n";
}

// ─── main ─────────────────────────────────────────────────────────────────────

int main() {
    std::cout << "=== test_core ===\n";

    test_dist_self_zero();
    test_dist_symmetric();
    test_dist_known_value();
    test_route_demand();
    test_route_length();
    test_validate_ok();
    test_validate_capacity();
    test_validate_duplicate();
    test_validate_missing();
    test_single_client();

    std::cout << "All core tests passed.\n";
    return 0;
}
