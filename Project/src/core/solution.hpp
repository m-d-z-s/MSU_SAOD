#pragma once

#include "instance.hpp"
#include "distance.hpp"
#include <vector>
#include <string>

namespace vrp {

/**
 * @brief Один маршрут транспортного средства.
 *
 * Хранит последовательность индексов клиентов (без депо на концах).
 * Депо подразумевается в начале и конце при расчёте стоимости.
 */
struct Route {
    std::vector<int> clients; ///< Индексы клиентов (1-based, без депо)

    /** @brief Суммарный спрос маршрута. */
    int demand(const Instance& inst) const;

    /** @brief Длина маршрута (с возвратом в депо). */
    double length(const DistanceMatrix& dist) const;

    bool empty() const { return clients.empty(); }
    int  size()  const { return static_cast<int>(clients.size()); }
};

/**
 * @brief Полное решение задачи VRP.
 *
 * Содержит набор маршрутов и ссылки на инстанс и матрицу расстояний.
 */
struct Solution {
    std::vector<Route> routes; ///< Маршруты (пустые маршруты допустимы)

    /** @brief Суммарная длина всех маршрутов. */
    double total_length(const DistanceMatrix& dist) const;

    /** @brief Число непустых маршрутов. */
    int num_routes() const;

    /**
     * @brief Проверяет допустимость решения.
     *
     * Условия: каждый клиент посещён ровно один раз,
     * спрос каждого маршрута не превышает вместимость.
     * @param inst  Инстанс задачи.
     * @return Пустая строка если OK, иначе описание ошибки.
     */
    std::string validate(const Instance& inst) const;
};

} // namespace vrp
