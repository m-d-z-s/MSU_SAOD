#pragma once

#include "core/instance.hpp"
#include "core/distance.hpp"
#include "core/solution.hpp"

namespace vrp {

/**
 * @brief Параметры алгоритма
 */
struct SAParams {
    double t_initial   = 100.0; ///< Начальная температура
    double t_min       = 0.01;  ///< Минимальная температура (стоп-критерий)
    double alpha       = 0.995; ///< Коэффициент охлаждения: T *= alpha каждый шаг
    int    max_iter    = 50000; ///< Максимум итераций без улучшения глобального лучшего
    int    plateau_len = 500;   ///< Длина «плато»: нет улучшений → alpha адаптируется
    double alpha_boost = 0.98;  ///< alpha при плато (замедляем охлаждение)
    double vehicle_penalty = 10.0; ///< Штраф за одно лишнее ТС в функции оценки
    unsigned seed = 42;         ///< Начальное значение
};

/**
 * @brief Алгоритм имитации отжига (Simulated Annealing) для VRP.
 *
 * Метаэвристика поверх локального поиска:
 *   - Начальное решение: Clarke-Wright (передаётся извне).
 *   - Соседство: случайный выбор из {2-opt, or-opt(k=1), relocate}.
 *   - Принятие ухудшающего хода: exp(-delta / T) > random[0,1).
 *   - Охлаждение: геометрическое T *= alpha; при плато alpha адаптируется.
 *   - Функция оценки: total_length + vehicle_penalty * num_routes.
 *
 * Адаптивное охлаждение:
 *   Если за plateau_len итераций глобальный лучший результат не улучшился,
 *   alpha временно заменяется на alpha_boost (медленнее остываем),
 *   давая алгоритму больше времени на выход из локального оптимума.
 *
 * Сложность: O(max_iter * L) в среднем, где L — длина маршрута.
 */
class SimulatedAnnealing {
public:
    /**
     * @brief Запускает SA-оптимизацию над переданным решением.
     *
     * @param sol    Начальное решение (модифицируется in-place до лучшего найденного).
     * @param inst   Инстанс задачи.
     * @param dist   Предвычисленная матрица расстояний.
     * @param params Параметры алгоритма.
     */
    static void optimize(Solution& sol, const Instance& inst,
                         const DistanceMatrix& dist, const SAParams& params = {});
};

} // namespace vrp
