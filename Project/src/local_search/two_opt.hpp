#pragma once

#include "core/instance.hpp"
#include "core/distance.hpp"
#include "core/solution.hpp"

namespace vrp {

/**
 * @brief Локальный поиск 2-opt для маршрутов VRP.
 *
 * Оператор 2-opt переворачивает подотрезок клиентов внутри одного маршрута.
 * Для маршрута [0, c0, c1, ..., ci, ci+1, ..., cj, cj+1, ..., 0]
 * операция 2-opt(i, j) переворачивает сегмент [i+1..j]:
 *   было:  ... -> ci -> ci+1 -> ... -> cj -> cj+1 -> ...
 *   стало: ... -> ci -> cj -> ... -> ci+1 -> cj+1 -> ...
 *
 * Стратегия: «first improvement» — принимаем первое найденное улучшение,
 * повторяем до локального оптимума.
 *
 * Delta evaluation (горячий путь):
 *   delta = dist[ci][cj] + dist[ci+1][cj+1]
 *         - dist[ci][ci+1] - dist[cj][cj+1]
 * Использует предвычисленную матрицу — без повторных sqrt().
 *
 * Сложность одного прохода: O(L^2) на маршрут, O(m * L^2) суммарно.
 */
class TwoOpt {
public:
    /**
     * @brief Улучшает все маршруты решения оператором 2-opt.
     *
     * @param sol   Текущее решение (модифицируется in-place).
     * @param inst  Инстанс задачи (не используется, для единообразия API).
     * @param dist  Предвычисленная матрица расстояний.
     * @return Суммарное уменьшение длины (>= 0).
     */
    static double improve(Solution& sol, const Instance& inst,
                          const DistanceMatrix& dist);

    /**
     * @brief Применяет 2-opt к одному маршруту до локального оптимума.
     *
     * @param route Маршрут (модифицируется in-place).
     * @param dist  Предвычисленная матрица расстояний.
     * @return Суммарное уменьшение длины маршрута (>= 0).
     */
    static double improve_route(Route& route, const DistanceMatrix& dist);
};

} // namespace vrp
