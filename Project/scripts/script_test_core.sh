g++ -std=c++17 -Wall \
  src/core/distance.cpp \
  src/core/solution.cpp \
  tests/test_core.cpp \
  -I src -o test_core

./test_core