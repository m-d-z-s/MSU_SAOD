#pragma once

#include "core/instance.hpp"
#include "core/distance.hpp"
#include "core/solution.hpp"

namespace vrp {

    /**
     * @brief Параметры алгоритма поиска с запретами.
     */
    struct TSParams {
        int    max_iter      = 500;  ///< Максимум итераций
        int    tabu_tenure   = 10;   ///< Длина запрета: ход запрещён на tabu_tenure итераций
        int    max_no_improve = 100; ///< Стоп: итераций без улучшения глобального лучшего
        double vehicle_penalty = 10.0; ///< Штраф за одно ТС в функции оценки
    };

    /**
     * @brief Алгоритм поиска с запретами (Tabu Search) для VRP.
     *
     * На каждой итерации перебираются все допустимые ходы типа Relocate
     * (перенос клиента между маршрутами) и Swap (обмен клиентов).
     * Выбирается лучший ход, не находящийся в списке запретов.
     *
     * Список запретов (tabu list): запоминаем пару (client, route) —
     * клиент не может вернуться в исходный маршрут в течение tabu_tenure итераций.
     *
     * Критерий аспирации: запрещённый ход принимается, если он улучшает
     * глобальный лучший результат.
     *
     * Функция оценки: total_length + vehicle_penalty * num_routes.
     *
     * Сложность одной итерации: O(m² * L²), где m — число маршрутов, L — длина.
     *
     * @see Gendreau et al. (1994) — tabu search для CVRP.
     */
    class TabuSearch {
    public:
        /**
         * @brief Запускает TS-оптимизацию над переданным решением.
         *
         * @param sol    Начальное решение (модифицируется до лучшего найденного).
         * @param inst   Инстанс задачи.
         * @param dist   Предвычисленная матрица расстояний.
         * @param params Параметры алгоритма.
         */
        static void optimize(Solution& sol, const Instance& inst,
                             const DistanceMatrix& dist, const TSParams& params = {});
    };

} // namespace vrp

