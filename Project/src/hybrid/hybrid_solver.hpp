#pragma once

#include "core/instance.hpp"
#include "core/distance.hpp"
#include "core/solution.hpp"
#include "metaheuristics/simulated_annealing.hpp"

namespace vrp {

/**
 * @brief Параметры гибридного решателя.
 *
 * Гибрид реализует собственную модификацию, требуемую преподавателем:
 *   Clarke-Wright → 2-opt → SA с адаптивным охлаждением.
 *
 * Функция оценки: total_length + vehicle_penalty * num_routes.
 */
struct HybridParams {
    bool  use_two_opt       = true;   ///< Применять 2-opt перед SA
    bool  use_or_opt        = true;   ///< Применять Or-opt перед SA
    bool  use_inter_route   = true;   ///< Применять межмаршрутный поиск перед SA
    SAParams sa;                      ///< Параметры Simulated Annealing
};

/**
 * @brief Гибридный решатель VRP — собственная модификация.
 *
 * Алгоритм (pipeline):
 *   1. **Clarke-Wright** (параллельная версия) — конструктивная эвристика.
 *      Даёт хорошее стартовое решение за O(n² log n).
 *   2. **2-opt** — внутримаршрутное улучшение до локального оптимума.
 *      Устраняет «перекрёстки» внутри маршрутов.
 *   3. **Or-opt** (k=1,2,3) — перестановка сегментов внутри маршрута.
 *      Дополняет 2-opt, находя улучшения другого типа.
 *   4. **Relocate + Swap** — межмаршрутный локальный поиск.
 *      Перебалансирует нагрузку между маршрутами.
 *   5. **SA с адаптивным охлаждением** — метаэвристика для выхода
 *      из локальных оптимумов с использованием взвешенной функции оценки.
 *
 * Взвешенная функция оценки (обоснование эффективности):
 *   score = total_length + vehicle_penalty * num_routes
 *   Штраф за машины побуждает алгоритм объединять маршруты,
 *   одновременно минимизируя суммарный пробег. Это позволяет
 *   оптимизировать два критерия (стоимость + число ТС) в едином
 *   скалярном критерии без многокритериальной оптимизации.
 *
 * Эффективность гибрида:
 *   - CW даёт решение в пределах 10-20% от оптимума.
 *   - Локальный поиск (2-opt + Or-opt + межмаршрутный) улучшает на 5-15%.
 *   - SA вырывается из локальных оптимумов; адаптивное охлаждение
 *     замедляет остывание при «плато», давая больше времени на поиск.
 *   - Итого: типичное качество 5-10% от оптимума на стандартных бенчмарках.
 *
 * @see Clarke & Wright (1964); Kirkpatrick et al. (1983); Gendreau et al. (1994).
 */
class HybridSolver {
public:
    /**
     * @brief Решает задачу CVRP гибридным методом.
     *
     * @param inst   Инстанс задачи.
     * @param dist   Предвычисленная матрица расстояний.
     * @param params Параметры гибрида (включая параметры SA).
     * @return Лучшее найденное допустимое решение.
     */
    static Solution solve(const Instance& inst, const DistanceMatrix& dist,
                          const HybridParams& params = {});

    /**
     * @brief Оценивает качество решения взвешенной функцией.
     *
     * score = total_length + vehicle_penalty * num_routes
     *
     * @param sol            Решение.
     * @param dist           Матрица расстояний.
     * @param vehicle_penalty Штраф за одно транспортное средство.
     * @return Скалярная оценка (меньше — лучше).
     */
    static double score(const Solution& sol, const DistanceMatrix& dist,
                        double vehicle_penalty = 10.0);
};

} // namespace vrp
