g++ -std=c++17 -Wall \
  src/core/distance.cpp \
  src/core/solution.cpp \
  src/parsers/vrp_parser.cpp \
  src/parsers/solomon_parser.cpp \
  tests/test_parsers.cpp \
  -I src -o test_parsers

./test_parsers