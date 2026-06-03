#!/bin/bash
g++ -std=c++17 -Wall \
  src/core/distance.cpp \
  src/core/solution.cpp \
  src/heuristics/clarke_wright.cpp \
  src/heuristics/nearest_neighbor.cpp \
  tests/test_heuristics.cpp \
  -I src -o test_heuristics

./test_heuristics