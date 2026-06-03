#pragma once

#include "core/instance.hpp"
#include "core/distance.hpp"
#include "core/solution.hpp"

namespace vrp {

/**
 * @brief Межмаршрутные операторы локального поиска: Relocate и Swap.
 *
 * **Relocate** (перенос одного клиента):
 *   Переносит клиента c из маршрута r1 в лучшую позицию маршрута r2 (r2 ≠ r1).
 *   Условие: спрос маршрута r2 после вставки ≤ вместимость.
 *   Delta = cost_remove(c, r1) + cost_insert(c, best_pos, r2).
 *
 * **Swap** (обмен двух клиентов):
 *   Обменивает клиентов ci (из r1) и cj (из r2) местами.
 *   Условие: вместимости обоих маршрутов соблюдаются после обмена.
 *   Delta = delta_remove_ci_r1 + delta_insert_cj_r1
 *         + delta_remove_cj_r2 + delta_insert_ci_r2.
 *
 * Стратегия: «best improvement» по всем парам маршрутов и клиентов.
 * Пустые маршруты удаляются после каждого прохода.
 *
 * Сложность одного прохода: O(m² * L²), где m — число маршрутов.
 */
class InterRoute {
public:
    /**
     * @brief Применяет оператор Relocate ко всем парам маршрутов.
     *
     * @param sol   Текущее решение (модифицируется in-place).
     * @param inst  Инстанс задачи (для проверки вместимости).
     * @param dist  Предвычисленная матрица расстояний.
     * @return Суммарное уменьшение длины (>= 0).
     */
    static double relocate(Solution& sol, const Instance& inst,
                           const DistanceMatrix& dist);

    /**
     * @brief Применяет оператор Swap ко всем парам маршрутов.
     *
     * @param sol   Текущее решение (модифицируется in-place).
     * @param inst  Инстанс задачи (для проверки вместимости).
     * @param dist  Предвычисленная матрица расстояний.
     * @return Суммарное уменьшение длины (>= 0).
     */
    static double swap(Solution& sol, const Instance& inst,
                       const DistanceMatrix& dist);

    /**
     * @brief Применяет Relocate и Swap поочерёдно до локального оптимума.
     *
     * @param sol   Текущее решение (модифицируется in-place).
     * @param inst  Инстанс задачи.
     * @param dist  Предвычисленная матрица расстояний.
     * @return Суммарное уменьшение длины (>= 0).
     */
    static double improve(Solution& sol, const Instance& inst,
                          const DistanceMatrix& dist);
};

} // namespace vrp
