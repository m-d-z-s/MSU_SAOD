#pragma once

#include "core/instance.hpp"
#include "core/distance.hpp"
#include "core/solution.hpp"

namespace vrp {

/**
 * @brief Параллельный алгоритм экономии Кларка-Райта (Clarke-Wright Savings).
 *
 * Алгоритм (параллельная версия, Clarke & Wright, 1964):
 *   1. Стартовое решение: каждый клиент — отдельный маршрут (депо -> i -> депо).
 *   2. Для каждой пары клиентов (i, j) вычисляется экономия:
 *        s(i,j) = dist[0][i] + dist[0][j] - dist[i][j]
 *   3. Пары сортируются по убыванию экономии.
 *   4. Жадно объединяем маршруты: если i — конец одного маршрута,
 *      j — начало другого, и суммарный спрос не превышает вместимость,
 *      маршруты сливаются.
 *
 * Сложность: O(n^2 log n) — доминирует сортировка пар экономий.
 *
 * Это стартовое решение для метаэвристик (SA, Tabu Search).
 */
class ClarkeWright {
public:
    /**
     * @brief Строит решение CVRP алгоритмом экономии Кларка-Райта.
     * @param inst  Инстанс задачи.
     * @param dist  Предвычисленная матрица расстояний.
     * @return Допустимое решение.
     */
    static Solution solve(const Instance& inst, const DistanceMatrix& dist);
};

} // namespace vrp
