#include "metaheuristics/simulated_annealing.hpp"
#include "local_search/two_opt.hpp"
#include "local_search/or_opt.hpp"
#include "local_search/inter_route.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

namespace vrp {

namespace {

/**
 * @brief Взвешенная функция оценки решения.
 *
 * cost = total_length + vehicle_penalty * num_routes
 *
 * Штраф за число машин стимулирует алгоритм уменьшать парк ТС —
 * это соответствует реальным задачам, где аренда машины стоит дороже пробега.
 */
double cost(const Solution& sol, const DistanceMatrix& dist, double penalty) {
    return sol.total_length(dist)
         + penalty * static_cast<double>(sol.num_routes());
}

// ─── Операторы соседства ──────────────────────────────────────────────────────

/**
 * @brief Применяет 2-opt к случайному непустому маршруту.
 *
 * Один проход 2-opt (first improvement) на выбранном маршруте.
 * Возвращает изменение суммарной длины (< 0 — улучшение).
 */
double neighbor_two_opt(Solution& sol, const DistanceMatrix& dist,
                        std::mt19937& rng) {
    // Собираем непустые маршруты.
    std::vector<int> nonempty;
    nonempty.reserve(sol.routes.size());
    for (int i = 0; i < static_cast<int>(sol.routes.size()); ++i) {
        if (!sol.routes[i].empty()) nonempty.push_back(i);
    }
    if (nonempty.empty()) return 0.0;

    std::uniform_int_distribution<int> pick(0, static_cast<int>(nonempty.size()) - 1);
    Route& r = sol.routes[nonempty[pick(rng)]];

    double before = r.length(dist);
    TwoOpt::improve_route(r, dist);
    return r.length(dist) - before;
}

/**
 * @brief Применяет or-opt(k=1) к случайному маршруту.
 */
double neighbor_or_opt(Solution& sol, const DistanceMatrix& dist,
                       std::mt19937& rng) {
    std::vector<int> nonempty;
    nonempty.reserve(sol.routes.size());
    for (int i = 0; i < static_cast<int>(sol.routes.size()); ++i) {
        if (!sol.routes[i].empty()) nonempty.push_back(i);
    }
    if (nonempty.empty()) return 0.0;

    std::uniform_int_distribution<int> pick(0, static_cast<int>(nonempty.size()) - 1);
    Route& r = sol.routes[nonempty[pick(rng)]];

    double before = r.length(dist);
    OrOpt::improve_route(r, dist, 1);
    return r.length(dist) - before;
}

/**
 * @brief Пробует одиночный relocate: случайный клиент в случайный маршрут.
 *
 * В отличие от InterRoute::relocate (best improvement), здесь выбираем
 * конкретную пару (клиент, маршрут-приёмник) случайно, чтобы соседство
 * было быстрым и разнообразным.
 */
double neighbor_relocate(Solution& sol, const Instance& inst,
                         const DistanceMatrix& dist, std::mt19937& rng) {
    const int m = static_cast<int>(sol.routes.size());
    if (m < 2) return 0.0;

    // Выбираем случайный непустой маршрут-источник.
    std::vector<int> nonempty;
    for (int i = 0; i < m; ++i)
        if (!sol.routes[i].empty()) nonempty.push_back(i);
    if (nonempty.size() < 2) return 0.0;

    std::uniform_int_distribution<int> pick_r(0, static_cast<int>(nonempty.size()) - 1);
    int idx1 = pick_r(rng);
    int r1   = nonempty[idx1];

    // Выбираем случайного клиента из r1.
    Route& src = sol.routes[r1];
    std::uniform_int_distribution<int> pick_c(0, src.size() - 1);
    int pos1   = pick_c(rng);
    int client = src.clients[pos1];
    int demand = inst.clients[client].demand;

    // Выбираем случайный маршрут-приёмник (не r1).
    // Фильтруем по вместимости.
    std::vector<int> candidates;
    for (int r2 : nonempty) {
        if (r2 == r1) continue;
        if (sol.routes[r2].demand(inst) + demand <= inst.capacity)
            candidates.push_back(r2);
    }
    if (candidates.empty()) return 0.0;

    std::uniform_int_distribution<int> pick_r2(0, static_cast<int>(candidates.size()) - 1);
    int r2 = candidates[pick_r2(rng)];

    // Находим лучшую позицию вставки в r2.
    const auto& c2 = sol.routes[r2].clients;
    const int sz2  = static_cast<int>(c2.size());
    double best_ins_cost = std::numeric_limits<double>::infinity();
    int    best_ins_pos  = 0;

    for (int ins = 0; ins <= sz2; ++ins) {
        int before_node = (ins == 0)   ? 0 : c2[ins - 1];
        int after_node  = (ins == sz2) ? 0 : c2[ins];
        double c = dist(before_node, client) + dist(client, after_node)
                 - dist(before_node, after_node);
        if (c < best_ins_cost) { best_ins_cost = c; best_ins_pos = ins; }
    }

    // Вычисляем delta длины (без штрафа за машины — он считается снаружи).
    int prev1 = (pos1 == 0)          ? 0 : src.clients[pos1 - 1];
    int next1 = (pos1 == src.size()-1) ? 0 : src.clients[pos1 + 1];
    double remove_cost = dist(prev1, next1) - dist(prev1, client) - dist(client, next1);
    double delta_len = remove_cost + best_ins_cost;

    // Выполняем ход.
    src.clients.erase(src.clients.begin() + pos1);
    sol.routes[r2].clients.insert(
        sol.routes[r2].clients.begin() + best_ins_pos, client);

    return delta_len;
}

/**
 * @brief Глубокое копирование Solution (routes — vector of vectors).
 */
Solution clone(const Solution& sol) {
    return sol; // Route содержит только std::vector<int>, копируется корректно.
}

} // namespace

// ─── SimulatedAnnealing::optimize ─────────────────────────────────────────────

void SimulatedAnnealing::optimize(Solution& sol, const Instance& inst,
                                  const DistanceMatrix& dist,
                                  const SAParams& params) {
    std::mt19937 rng(params.seed);
    std::uniform_real_distribution<double> uni(0.0, 1.0);
    std::uniform_int_distribution<int>     pick_op(0, 2); // 3 оператора

    Solution best_sol  = clone(sol);
    double   best_cost = cost(best_sol, dist, params.vehicle_penalty);
    double   cur_cost  = best_cost;

    double T     = params.t_initial;
    double alpha = params.alpha;

    int iter_no_improve = 0; // итераций без улучшения глобального лучшего
    int plateau_count   = 0; // итераций без улучшения текущего (для плато)

    for (int iter = 0; T > params.t_min && iter_no_improve < params.max_iter; ++iter) {
        // Клонируем текущее решение — соседство модифицирует его in-place.
        Solution candidate = clone(sol);
        double   delta_len = 0.0;

        // Выбираем случайный оператор соседства.
        switch (pick_op(rng)) {
            case 0: delta_len = neighbor_two_opt(candidate, dist, rng);    break;
            case 1: delta_len = neighbor_or_opt(candidate, dist, rng);     break;
            case 2: delta_len = neighbor_relocate(candidate, inst, dist, rng); break;
            default: break;
        }

        // Пересчитываем полную стоимость (с учётом числа маршрутов).
        // Используем точный пересчёт, т.к. relocate может изменить num_routes.
        double new_cost = cost(candidate, dist, params.vehicle_penalty);
        double delta    = new_cost - cur_cost;

        // Критерий Метрополиса.
        bool accept = (delta < 0.0)
                   || (uni(rng) < std::exp(-delta / T));

        if (accept) {
            sol      = std::move(candidate);
            cur_cost = new_cost;
            ++plateau_count;

            if (cur_cost < best_cost - 1e-9) {
                best_sol         = clone(sol);
                best_cost        = cur_cost;
                iter_no_improve  = 0;
                plateau_count    = 0;
                alpha            = params.alpha; // сбрасываем alpha
            } else {
                ++iter_no_improve;
            }
        } else {
            ++iter_no_improve;
            ++plateau_count;
        }

        // Адаптивное охлаждение: если плато — замедляем остывание.
        if (plateau_count >= params.plateau_len) {
            alpha         = params.alpha_boost;
            plateau_count = 0;
        }

        T *= alpha;
    }

    // Возвращаем глобально лучшее решение.
    sol = std::move(best_sol);
}

} // namespace vrp
