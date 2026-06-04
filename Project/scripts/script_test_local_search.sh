#!/bin/bash
g++ -std=c++17 -Wall \
  src/core/distance.cpp \
  src/core/solution.cpp \
  src/heuristics/nearest_neighbor.cpp \
  src/heuristics/clarke_wright.cpp \
  src/local_search/two_opt.cpp \
  src/local_search/or_opt.cpp \
  src/local_search/inter_route.cpp \
  tests/test_local_search.cpp \
  -I src -o test_local_search

./test_local_search