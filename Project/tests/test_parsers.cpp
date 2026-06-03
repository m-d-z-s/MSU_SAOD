/**
 * @brief Тесты парсеров VRP и Solomon.
 *
 * Создаём тестовые файлы во временной директории,
 * парсим их и проверяем корректность загруженного инстанса.
 */

#include "parsers/vrp_parser.hpp"
#include "parsers/solomon_parser.hpp"
#include <iostream>
#include <fstream>
#include <cassert>
#include <stdexcept>
#include <cmath>

// ── Утилиты ──────────────────────────────────────────────────────────────────

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

static void write_file(const std::string& path, const std::string& content) {
    std::ofstream f(path);
    f << content;
}

// ── Тесты VrpParser ──────────────────────────────────────────────────────────

static void test_vrp_basic() {
    std::cout << "\n[VrpParser] basic parse\n";

    const std::string path = "/tmp/test_basic.vrp";
    write_file(path,
        "NAME : test-basic\n"
        "COMMENT : simple 3-client\n"
        "TYPE : CVRP\n"
        "DIMENSION : 4\n"
        "EDGE_WEIGHT_TYPE : EUC_2D\n"
        "CAPACITY : 100\n"
        "NODE_COORD_SECTION\n"
        "1   0   0\n"
        "2  10   0\n"
        "3  10  10\n"
        "4   0  10\n"
        "DEMAND_SECTION\n"
        "1 0\n"
        "2 30\n"
        "3 40\n"
        "4 20\n"
        "DEPOT_SECTION\n"
        "1\n"
        "-1\n"
        "EOF\n"
    );

    vrp::Instance inst = vrp::VrpParser::parse(path);

    check(inst.name == "test-basic",      "name parsed");
    check(inst.capacity == 100,           "capacity == 100");
    check(inst.num_clients() == 3,        "3 clients (excl. depot)");
    check(inst.clients[0].demand == 0,    "depot demand == 0");
    check(inst.clients[0].x == 0.0,      "depot x == 0");
    check(inst.clients[1].x == 10.0,     "client[1].x == 10");
    check(inst.clients[1].demand == 30,  "client[1].demand == 30");
    check(inst.clients[3].demand == 20,  "client[3].demand == 20");
}

static void test_vrp_single_client() {
    std::cout << "\n[VrpParser] single client boundary\n";

    const std::string path = "/tmp/test_single.vrp";
    write_file(path,
        "NAME : one\n"
        "DIMENSION : 2\n"
        "CAPACITY : 50\n"
        "NODE_COORD_SECTION\n"
        "1 0 0\n"
        "2 5 5\n"
        "DEMAND_SECTION\n"
        "1 0\n"
        "2 10\n"
        "DEPOT_SECTION\n"
        "1\n"
        "-1\n"
        "EOF\n"
    );

    vrp::Instance inst = vrp::VrpParser::parse(path);
    check(inst.num_clients() == 1,      "1 client");
    check(inst.clients[1].demand == 10, "client demand == 10");
}

static void test_vrp_file_not_found() {
    std::cout << "\n[VrpParser] missing file throws\n";
    bool threw = false;
    try {
        vrp::VrpParser::parse("/tmp/no_such_file.vrp");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw, "throws for missing file");
}

// ── Тесты SolomonParser ──────────────────────────────────────────────────────

static void test_solomon_basic() {
    std::cout << "\n[SolomonParser] basic parse\n";

    const std::string path = "/tmp/test_solomon.txt";
    write_file(path,
        "C101\n"
        "\n"
        "VEHICLE\n"
        "NUMBER     CAPACITY\n"
        "25         200\n"
        "\n"
        "CUSTOMER\n"
        "CUST NO.  XCOORD.  YCOORD.  DEMAND  READY TIME  DUE DATE  SERVICE TIME\n"
        "\n"
        "0   40   50    0     0  1236     0\n"
        "1   45   68   10   912   967    90\n"
        "2   45   70   30   825   870    90\n"
    );

    vrp::Instance inst = vrp::SolomonParser::parse(path);

    check(inst.name == "C101",           "name == C101");
    check(inst.num_vehicles == 25,       "num_vehicles == 25");
    check(inst.capacity == 200,          "capacity == 200");
    check(inst.num_clients() == 2,       "2 clients");
    check(inst.clients[0].demand == 0,   "depot demand == 0");
    check(inst.clients[0].x == 40.0,    "depot x == 40");
    check(inst.clients[0].due == 1236,   "depot due == 1236");
    check(inst.clients[1].demand == 10,  "client[1].demand == 10");
    check(inst.clients[1].ready == 912,  "client[1].ready == 912");
    check(inst.clients[1].service == 90, "client[1].service == 90");
    check(inst.clients[2].demand == 30,  "client[2].demand == 30");
}

static void test_solomon_single_client() {
    std::cout << "\n[SolomonParser] single client boundary\n";

    const std::string path = "/tmp/test_solomon_one.txt";
    write_file(path,
        "R101\n"
        "\n"
        "VEHICLE\n"
        "NUMBER     CAPACITY\n"
        "3          80\n"
        "\n"
        "CUSTOMER\n"
        "CUST NO.  XCOORD.  YCOORD.  DEMAND  READY TIME  DUE DATE  SERVICE TIME\n"
        "\n"
        "0    0    0     0     0   230    0\n"
        "1   10   10    15     0   100   10\n"
    );

    vrp::Instance inst = vrp::SolomonParser::parse(path);
    check(inst.num_clients() == 1,       "1 client");
    check(inst.clients[1].demand == 15,  "client demand == 15");
}

static void test_solomon_file_not_found() {
    std::cout << "\n[SolomonParser] missing file throws\n";
    bool threw = false;
    try {
        vrp::SolomonParser::parse("/tmp/no_such_file_solomon.txt");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw, "throws for missing file");
}

// ── main ─────────────────────────────────────────────────────────────────────

int main() {
    std::cout << "=== test_parsers ===\n";

    test_vrp_basic();
    test_vrp_single_client();
    test_vrp_file_not_found();

    test_solomon_basic();
    test_solomon_single_client();
    test_solomon_file_not_found();

    std::cout << "\n--- Results: " << passed << " passed, " << failed << " failed ---\n";
    return failed == 0 ? 0 : 1;
}
