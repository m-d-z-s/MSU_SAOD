cmake_minimum_required(VERSION 3.16)
project(vrp CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── core library ──────────────────────────────────────────────────────────────
add_library(vrp_core
        src/core/distance.cpp
        src/core/solution.cpp
)
target_include_directories(vrp_core PUBLIC src)

# ── parsers library ───────────────────────────────────────────────────────────
add_library(vrp_parsers
        src/parsers/vrp_parser.cpp
        src/parsers/solomon_parser.cpp
)
target_link_libraries(vrp_parsers PUBLIC vrp_core)

# ── heuristics library ────────────────────────────────────────────────────────
add_library(vrp_heuristics
        src/heuristics/nearest_neighbor.cpp
        src/heuristics/clarke_wright.cpp
)
target_link_libraries(vrp_heuristics PUBLIC vrp_core)

# ── local search library ──────────────────────────────────────────────────────
add_library(vrp_local_search
        src/local_search/two_opt.cpp
        src/local_search/or_opt.cpp
        src/local_search/inter_route.cpp
)
target_link_libraries(vrp_local_search PUBLIC vrp_core)

# ── metaheuristics library ────────────────────────────────────────────────────
add_library(vrp_meta
        src/metaheuristics/simulated_annealing.cpp
        src/metaheuristics/tabu_search.cpp
)
target_link_libraries(vrp_meta PUBLIC vrp_core vrp_local_search)

# ── hybrid library ────────────────────────────────────────────────────────────
add_library(vrp_hybrid
        src/hybrid/hybrid_solver.cpp
)
target_link_libraries(vrp_hybrid PUBLIC vrp_core vrp_heuristics vrp_local_search vrp_meta)

# ── main executable ───────────────────────────────────────────────────────────
add_executable(vrp_solver src/main.cpp)
target_link_libraries(vrp_solver PRIVATE
        vrp_core vrp_parsers vrp_heuristics vrp_local_search vrp_meta vrp_hybrid)
# std::filesystem needs explicit linkage on GCC < 9 (Linux)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9.0")
    target_link_libraries(vrp_solver PRIVATE stdc++fs)
endif()

# ── tests ─────────────────────────────────────────────────────────────────────
add_executable(test_core    tests/test_core.cpp)
target_link_libraries(test_core    PRIVATE vrp_core)

add_executable(test_parsers tests/test_parsers.cpp)
target_link_libraries(test_parsers PRIVATE vrp_core vrp_parsers)

add_executable(test_heuristics tests/test_heuristics.cpp)
target_link_libraries(test_heuristics PRIVATE vrp_core vrp_heuristics)

add_executable(test_local_search tests/test_local_search.cpp)
target_link_libraries(test_local_search PRIVATE vrp_core vrp_heuristics vrp_local_search)

add_executable(test_metaheuristics tests/test_metaheuristics.cpp)
target_link_libraries(test_metaheuristics PRIVATE vrp_core vrp_heuristics vrp_local_search vrp_meta)

add_executable(test_hybrid tests/test_hybrid.cpp)
target_link_libraries(test_hybrid PRIVATE vrp_core vrp_heuristics vrp_local_search vrp_meta vrp_hybrid)

# ── benchmarks ────────────────────────────────────────────────────────────────
add_executable(run_benchmarks benchmarks/run_benchmarks.cpp)
target_link_libraries(run_benchmarks PRIVATE
        vrp_core vrp_parsers vrp_heuristics vrp_local_search vrp_meta vrp_hybrid)


# ── svg visualizer ────────────────────────────────────────────────────────────
add_executable(viz_svg viz/viz_svg.cpp)
target_link_libraries(viz_svg PRIVATE)
target_include_directories(viz_svg PRIVATE src)
if(NOT MSVC)
    target_compile_options(viz_svg PRIVATE -Wall -Wextra -Wpedantic)
endif()
# ── warnings ──────────────────────────────────────────────────────────────────
if(MSVC)
    target_compile_options(vrp_core PRIVATE /W4)
else()
    foreach(tgt vrp_core vrp_parsers vrp_heuristics vrp_local_search vrp_meta vrp_hybrid
            vrp_solver test_core test_parsers test_heuristics test_local_search
            test_metaheuristics test_hybrid run_benchmarks)
        target_compile_options(${tgt} PRIVATE -Wall -Wextra -Wpedantic)
    endforeach()
endif()



#cmake_minimum_required(VERSION 3.16)
#project(vrp CXX)
#
#set(CMAKE_CXX_STANDARD 17)
#set(CMAKE_CXX_STANDARD_REQUIRED ON)
#
## ── core library ──────────────────────────────────────────────────────────────
#add_library(vrp_core
#        src/core/distance.cpp
#        src/core/solution.cpp
#)
#target_include_directories(vrp_core PUBLIC src)
#
## ── parsers library ───────────────────────────────────────────────────────────
#add_library(vrp_parsers
#        src/parsers/vrp_parser.cpp
#        src/parsers/solomon_parser.cpp
#)
#target_link_libraries(vrp_parsers PUBLIC vrp_core)
#
## ── heuristics library ────────────────────────────────────────────────────────
#add_library(vrp_heuristics
#        src/heuristics/nearest_neighbor.cpp
#        src/heuristics/clarke_wright.cpp
#)
#target_link_libraries(vrp_heuristics PUBLIC vrp_core)
#
## ── local search library ──────────────────────────────────────────────────────
#add_library(vrp_local_search
#        src/local_search/two_opt.cpp
#        src/local_search/or_opt.cpp
#        src/local_search/inter_route.cpp
#)
#target_link_libraries(vrp_local_search PUBLIC vrp_core)
#
## ── metaheuristics library ────────────────────────────────────────────────────
#add_library(vrp_meta
#        src/metaheuristics/simulated_annealing.cpp
#        src/metaheuristics/tabu_search.cpp
#)
#target_link_libraries(vrp_meta PUBLIC vrp_core vrp_local_search)
#
## ── hybrid library ────────────────────────────────────────────────────────────
#add_library(vrp_hybrid
#        src/hybrid/hybrid_solver.cpp
#)
#target_link_libraries(vrp_hybrid PUBLIC vrp_core vrp_heuristics vrp_local_search vrp_meta)
#
## ── main executable ───────────────────────────────────────────────────────────
#add_executable(vrp_solver src/main.cpp)
#target_link_libraries(vrp_solver PRIVATE
#        vrp_core vrp_parsers vrp_heuristics vrp_local_search vrp_meta vrp_hybrid)
#
## ── tests ─────────────────────────────────────────────────────────────────────
#add_executable(test_core    tests/test_core.cpp)
#target_link_libraries(test_core    PRIVATE vrp_core)
#
#add_executable(test_parsers tests/test_parsers.cpp)
#target_link_libraries(test_parsers PRIVATE vrp_core vrp_parsers)
#
#add_executable(test_heuristics tests/test_heuristics.cpp)
#target_link_libraries(test_heuristics PRIVATE vrp_core vrp_heuristics)
#
#add_executable(test_local_search tests/test_local_search.cpp)
#target_link_libraries(test_local_search PRIVATE vrp_core vrp_heuristics vrp_local_search)
#
#add_executable(test_metaheuristics tests/test_metaheuristics.cpp)
#target_link_libraries(test_metaheuristics PRIVATE vrp_core vrp_heuristics vrp_local_search vrp_meta)
#
#add_executable(test_hybrid tests/test_hybrid.cpp)
#target_link_libraries(test_hybrid PRIVATE vrp_core vrp_heuristics vrp_local_search vrp_meta vrp_hybrid)
#
## ── benchmarks ────────────────────────────────────────────────────────────────
#add_executable(run_benchmarks benchmarks/run_benchmarks.cpp)
#target_link_libraries(run_benchmarks PRIVATE
#        vrp_core vrp_parsers vrp_heuristics vrp_local_search vrp_meta vrp_hybrid)
#
#
## ── svg visualizer ────────────────────────────────────────────────────────────
#add_executable(viz_svg viz/viz_svg.cpp)
#target_link_libraries(viz_svg PRIVATE)
#target_include_directories(viz_svg PRIVATE src)
#if(NOT MSVC)
#    target_compile_options(viz_svg PRIVATE -Wall -Wextra -Wpedantic)
#endif()
## ── warnings ──────────────────────────────────────────────────────────────────
#if(MSVC)
#    target_compile_options(vrp_core PRIVATE /W4)
#else()
#    foreach(tgt vrp_core vrp_parsers vrp_heuristics vrp_local_search vrp_meta vrp_hybrid
#            vrp_solver tests_exe/test_core tests_exe/test_parsers tests_exe/test_heuristics tests_exe/test_local_search
#            tests_exe/test_metaheuristics tests_exe/test_hybrid run_benchmarks)
#        target_compile_options(${tgt} PRIVATE -Wall -Wextra -Wpedantic)
#    endforeach()
#endif()








#cmake_minimum_required(VERSION 3.16)
#project(vrp CXX)
#
#set(CMAKE_CXX_STANDARD 17)
#set(CMAKE_CXX_STANDARD_REQUIRED ON)
#
## ── core library ──────────────────────────────────────────────────────────────
#add_library(vrp_core
#        src/core/distance.cpp
#        src/core/solution.cpp
#)
#target_include_directories(vrp_core PUBLIC src)
#
## ── parsers library ───────────────────────────────────────────────────────────
#add_library(vrp_parsers
#        src/parsers/vrp_parser.cpp
#        src/parsers/solomon_parser.cpp
#)
#target_link_libraries(vrp_parsers PUBLIC vrp_core)
#
## ── heuristics library ────────────────────────────────────────────────────────
#add_library(vrp_heuristics
#        src/heuristics/nearest_neighbor.cpp
#        src/heuristics/clarke_wright.cpp
#)
#target_link_libraries(vrp_heuristics PUBLIC vrp_core)
#
## ── local search library ──────────────────────────────────────────────────────
#add_library(vrp_local_search
#        src/local_search/two_opt.cpp
#        src/local_search/or_opt.cpp
#        src/local_search/inter_route.cpp
#)
#target_link_libraries(vrp_local_search PUBLIC vrp_core)
#
## ── metaheuristics library ────────────────────────────────────────────────────
#add_library(vrp_meta
#        src/metaheuristics/simulated_annealing.cpp
#        src/metaheuristics/tabu_search.cpp
#)
#target_link_libraries(vrp_meta PUBLIC vrp_core vrp_local_search)
#
## ── hybrid library ────────────────────────────────────────────────────────────
#add_library(vrp_hybrid
#        src/hybrid/hybrid_solver.cpp
#)
#target_link_libraries(vrp_hybrid PUBLIC vrp_core vrp_heuristics vrp_local_search vrp_meta)
#
## ── main executable ───────────────────────────────────────────────────────────
#add_executable(vrp_solver src/main.cpp)
#target_link_libraries(vrp_solver PRIVATE
#        vrp_core vrp_parsers vrp_heuristics vrp_local_search vrp_meta vrp_hybrid)
#
## ── tests ─────────────────────────────────────────────────────────────────────
#add_executable(test_core    tests/test_core.cpp)
#target_link_libraries(test_core    PRIVATE vrp_core)
#
#add_executable(test_parsers tests/test_parsers.cpp)
#target_link_libraries(test_parsers PRIVATE vrp_core vrp_parsers)
#
#add_executable(test_heuristics tests/test_heuristics.cpp)
#target_link_libraries(test_heuristics PRIVATE vrp_core vrp_heuristics)
#
#add_executable(test_local_search tests/test_local_search.cpp)
#target_link_libraries(test_local_search PRIVATE vrp_core vrp_heuristics vrp_local_search)
#
#add_executable(test_metaheuristics tests/test_metaheuristics.cpp)
#target_link_libraries(test_metaheuristics PRIVATE vrp_core vrp_heuristics vrp_local_search vrp_meta)
#
#add_executable(test_hybrid tests/test_hybrid.cpp)
#target_link_libraries(test_hybrid PRIVATE vrp_core vrp_heuristics vrp_local_search vrp_meta vrp_hybrid)
#
## ── benchmarks ────────────────────────────────────────────────────────────────
#add_executable(run_benchmarks benchmarks/run_benchmarks.cpp)
#target_link_libraries(run_benchmarks PRIVATE
#        vrp_core vrp_parsers vrp_heuristics vrp_local_search vrp_meta vrp_hybrid)
#
## ── warnings ──────────────────────────────────────────────────────────────────
#if(MSVC)
#    target_compile_options(vrp_core PRIVATE /W4)
#else()
#    foreach(tgt vrp_core vrp_parsers vrp_heuristics vrp_local_search vrp_meta vrp_hybrid
#            vrp_solver test_core test_parsers test_heuristics test_local_search
#            test_metaheuristics test_hybrid run_benchmarks)
#        target_compile_options(${tgt} PRIVATE -Wall -Wextra -Wpedantic)
#    endforeach()
#endif()
