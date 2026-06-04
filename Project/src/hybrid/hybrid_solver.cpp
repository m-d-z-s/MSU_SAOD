/**
 * @file hybrid_solver.cpp
 * @brief Гибридный решатель VRP: Clarke-Wright → LS → SA.
 *
 * Собственная модификация.
 * Pipeline: CW -> 2-opt -> Or-opt -> Relocate+Swap -> SA (адаптивное охлаждение).
 * Функция оценки: total_length + vehicle_penalty * num_routes.
 */

#include "hybrid/hybrid_solver.hpp"
#include "heuristics/clarke_wright.hpp"
#include "local_search/two_opt.hpp"
#include "local_search/or_opt.hpp"
#include "local_search/inter_route.hpp"
#include "metaheuristics/simulated_annealing.hpp"

namespace vrp {

// ─── score ────────────────────────────────────────────────────────────────────

double HybridSolver::score(const Solution& sol, const DistanceMatrix& dist,
                           double vehicle_penalty) {
    return sol.total_length(dist)
           + vehicle_penalty * static_cast<double>(sol.num_routes());
}

// ─── solve ────────────────────────────────────────────────────────────────────

/**
 * @brief Реализация гибридного pipeline.
 *
 * Этапы:
 *   1. Clarke-Wright: строит начальное решение.
 *   2. Локальный поиск (2-opt, Or-opt, InterRoute): полирует решение.
 *   3. SA: выходит из локальных оптимумов с адаптивным охлаждением.
 *
 * Обоснование взвешенной функции оценки:
 *   Добавляя штраф vehicle_penalty * num_routes, стимулируем SA
 *   объединять маршруты (уменьшать число машин), что в реальных задачах
 *   снижает операционные расходы. Параметр vehicle_penalty позволяет
 *   регулировать компромисс между расстоянием и числом машин.
 *
 * Сложность:
 *   - CW: O(n^2 log n)
 *   - 2-opt: O(m * L^2) за проход, несколько итераций
 *   - Or-opt: O(m * L^2) за проход
 *   - InterRoute: O(m^2 * L^2) за проход
 *   - SA: O(max_iter) итераций, каждая O(L) для delta-evaluation
 */
Solution HybridSolver::solve(const Instance& inst, const DistanceMatrix& dist,
                             const HybridParams& params) {
    // ── Шаг 1: Конструктивная эвристика Clarke-Wright ──────────────────────
    Solution sol = ClarkeWright::solve(inst, dist);

    // ── Шаг 2: Локальный поиск ─────────────────────────────────────────────
    // Повторяем до тех пор, пока есть улучшения (VND-style).
    bool improved = true;
    while (improved) {
        improved = false;

        if (params.use_two_opt) {
            double gain = TwoOpt::improve(sol, inst, dist);
            if (gain > 1e-9) improved = true;
        }

        if (params.use_or_opt) {
            double gain = OrOpt::improve(sol, inst, dist);
            if (gain > 1e-9) improved = true;
        }

        if (params.use_inter_route) {
            double gain = InterRoute::improve(sol, inst, dist);
            if (gain > 1e-9) improved = true;
        }
    }

    // ── Шаг 3: SA с адаптивным охлаждением ────────────────────────────────
    // SA встроен в pipeline и использует ту же взвешенную функцию оценки,
    // что задаётся через params.sa.vehicle_penalty.
    SimulatedAnnealing::optimize(sol, inst, dist, params.sa);

    return sol;
}

} // namespace vrp
