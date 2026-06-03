#!/bin/bash
g++ -std=c++17 -Wall \
  src/core/distance.cpp \
  src/core/solution.cpp \
  src/heuristics/nearest_neighbor.cpp \
  src/heuristics/clarke_wright.cpp \
  src/local_search/two_opt.cpp \
  src/local_search/or_opt.cpp \
  src/local_search/inter_route.cpp \
  src/metaheuristics/simulated_annealing.cpp \
  src/metaheuristics/tabu_search.cpp \
  tests/test_metaheuristics.cpp \
  -I src -o test_metaheuristics

./test_metaheuristics