#pragma once

#include "core/instance.hpp"
#include "core/distance.hpp"
#include "core/solution.hpp"

namespace vrp {

/**
 * @brief Конструктивная эвристика «ближайший сосед» (Nearest Neighbor).
 *
 * Алгоритм:
 *   Пока есть непосещённые клиенты:
 *     1. Начать новый маршрут из депо.
 *     2. Жадно добавлять ближайшего непосещённого клиента,
 *        если его спрос вмещается в текущий маршрут.
 *     3. Если ни один клиент не вмещается — закрыть маршрут, начать новый.
 *
 * Сложность: O(n^2), где n — число клиентов.
 */
class NearestNeighbor {
public:
    /**
     * @brief Строит решение CVRP жадной эвристикой ближайшего соседа.
     * @param inst  Инстанс задачи.
     * @param dist  Предвычисленная матрица расстояний.
     * @return Допустимое решение (все клиенты посещены, вместимость соблюдена).
     */
    static Solution solve(const Instance& inst, const DistanceMatrix& dist);
};

} // namespace vrp
