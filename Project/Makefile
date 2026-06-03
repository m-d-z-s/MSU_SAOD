# Makefile для VRP Solver (альтернатива CMake)
CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Wpedantic -O2 -Isrc

CORE_SRC = src/core/distance.cpp src/core/solution.cpp
PARSER_SRC = src/parsers/vrp_parser.cpp src/parsers/solomon_parser.cpp
HEUR_SRC = src/heuristics/nearest_neighbor.cpp src/heuristics/clarke_wright.cpp
LS_SRC   = src/local_search/two_opt.cpp src/local_search/or_opt.cpp \
           src/local_search/inter_route.cpp
META_SRC = src/metaheuristics/simulated_annealing.cpp \
           src/metaheuristics/tabu_search.cpp
HYBRID_SRC = src/hybrid/hybrid_solver.cpp

ALL_LIB  = $(CORE_SRC) $(HEUR_SRC) $(LS_SRC) $(META_SRC) $(HYBRID_SRC)

.PHONY: all tests benchmarks clean

all: build/vrp_solver

build/vrp_solver: $(ALL_LIB) $(PARSER_SRC) src/main.cpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) $^ -o $@

tests: build/test_core build/test_parsers build/test_heuristics \
       build/test_local_search build/test_metaheuristics build/test_hybrid
	./build/test_core
	./build/test_parsers
	./build/test_heuristics
	./build/test_local_search
	./build/test_metaheuristics
	./build/test_hybrid

build/test_core: $(CORE_SRC) tests/test_core.cpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) $^ -o $@

build/test_parsers: $(CORE_SRC) $(PARSER_SRC) tests/test_parsers.cpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) $^ -o $@

build/test_heuristics: $(CORE_SRC) $(HEUR_SRC) tests/test_heuristics.cpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) $^ -o $@

build/test_local_search: $(CORE_SRC) $(HEUR_SRC) $(LS_SRC) tests/test_local_search.cpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) $^ -o $@

build/test_metaheuristics: $(CORE_SRC) $(HEUR_SRC) $(LS_SRC) $(META_SRC) \
                           tests/test_metaheuristics.cpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) $^ -o $@

build/test_hybrid: $(ALL_LIB) tests/test_hybrid.cpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) $^ -o $@

benchmarks: build/run_benchmarks
	./build/run_benchmarks results.csv

build/run_benchmarks: $(ALL_LIB) benchmarks/run_benchmarks.cpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) $^ -o $@

clean:
	rm -rf build results.csv
