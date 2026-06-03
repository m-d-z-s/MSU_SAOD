#pragma once

#include "core/instance.hpp"
#include "core/distance.hpp"
#include "core/solution.hpp"

namespace vrp {

/**
 * @brief Локальный поиск Or-opt для маршрутов VRP.
 *
 * Or-opt переносит цепочку из k клиентов (k = 1, 2, 3) из текущей позиции
 * в другую позицию того же маршрута (intra-route).
 *
 * Для каждого сегмента длины k и каждой позиции вставки вычисляется delta:
 *   delta = cost_remove(segment) + cost_insert(segment, pos)
 *         = [dist(prev, next_after_seg) - dist(prev, seg[0]) - dist(seg[-1], next_after_seg)]
 *         + [dist(ins_before, seg[0]) + dist(seg[-1], ins_after) - dist(ins_before, ins_after)]
 *
 * Стратегия: «best improvement» — выбираем лучший ход на каждой итерации.
 *
 * Сложность одного прохода: O(L^2) на маршрут для каждого k.
 */
class OrOpt {
public:
    /**
     * @brief Улучшает все маршруты решения оператором Or-opt.
     *
     * Применяет Or-opt для сегментов длины 1, 2 и 3 ко всем маршрутам.
     *
     * @param sol   Текущее решение (модифицируется in-place).
     * @param inst  Инстанс задачи (не используется для intra-route).
     * @param dist  Предвычисленная матрица расстояний.
     * @return Суммарное уменьшение длины (>= 0).
     */
    static double improve(Solution& sol, const Instance& inst,
                          const DistanceMatrix& dist);

    /**
     * @brief Применяет Or-opt к одному маршруту для сегментов длины k.
     *
     * @param route Маршрут (модифицируется in-place).
     * @param dist  Предвычисленная матрица расстояний.
     * @param k     Длина переносимого сегмента (1, 2 или 3).
     * @return Суммарное уменьшение длины маршрута (>= 0).
     */
    static double improve_route(Route& route, const DistanceMatrix& dist, int k);
};

} // namespace vrp
