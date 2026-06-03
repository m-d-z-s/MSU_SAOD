#include "metaheuristics/tabu_search.hpp"
#include "local_search/two_opt.hpp"

#include <vector>
#include <limits>
#include <algorithm>

namespace vrp {

namespace {

/**
 * @brief Запись в списке запретов.
 *
 * Запрещает возвращать client обратно в from_route в течение tenure итераций.
 * Используется для ходов Relocate и Swap.
 */
struct TabuEntry {
    int client;      ///< Перемещённый клиент
    int from_route;  ///< Маршрут, из которого клиент был удалён
    int expire_iter; ///< Итерация, после которой запрет снимается
};

/**
 * @brief Проверяет, находится ли ход (client, from_route) в списке запретов.
 */
bool is_tabu(const std::vector<TabuEntry>& tabu_list,
             int client, int from_route, int cur_iter) {
    for (const TabuEntry& e : tabu_list) {
        if (e.client == client && e.from_route == from_route
                && e.expire_iter > cur_iter) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Добавляет запись в список запретов, удаляя устаревшие.
 */
void add_tabu(std::vector<TabuEntry>& tabu_list,
              int client, int from_route, int cur_iter, int tenure) {
    // Удаляем истёкшие записи.
    tabu_list.erase(
        std::remove_if(tabu_list.begin(), tabu_list.end(),
                       [cur_iter](const TabuEntry& e) {
                           return e.expire_iter <= cur_iter;
                       }),
        tabu_list.end());
    tabu_list.push_back({client, from_route, cur_iter + tenure});
}

/**
 * @brief Взвешенная функция оценки решения.
 */
double eval(const Solution& sol, const DistanceMatrix& dist, double penalty) {
    return sol.total_length(dist)
         + penalty * static_cast<double>(sol.num_routes());
}

/**
 * @brief Описание хода Relocate: перенос клиента из одного маршрута в другой.
 */
struct RelocateMove {
    int    r1;       ///< Маршрут-источник
    int    pos1;     ///< Позиция клиента в r1
    int    client;   ///< Перемещаемый клиент
    int    r2;       ///< Маршрут-приёмник
    int    ins_pos;  ///< Позиция вставки в r2
    double delta;    ///< Изменение стоимости (< 0 — улучшение)
};

/**
 * @brief Описание хода Swap: обмен клиентов между двумя маршрутами.
 */
struct SwapMove {
    int    r1, p1, c1; ///< Маршрут/позиция/клиент 1
    int    r2, p2, c2; ///< Маршрут/позиция/клиент 2
    double delta;       ///< Изменение стоимости
};

/**
 * @brief Вычисляет стоимость вставки client в маршрут route на позицию ins.
 */
double insertion_cost(const Route& route, int client, int ins,
                      const DistanceMatrix& dist) {
    const auto& c  = route.clients;
    const int   sz = static_cast<int>(c.size());
    int before = (ins == 0)  ? 0 : c[ins - 1];
    int after  = (ins == sz) ? 0 : c[ins];
    return dist(before, client) + dist(client, after) - dist(before, after);
}

/**
 * @brief Вычисляет стоимость удаления клиента из позиции pos в маршруте.
 */
double removal_cost(const Route& route, int pos, const DistanceMatrix& dist) {
    const auto& c  = route.clients;
    const int   sz = static_cast<int>(c.size());
    int prev = (pos == 0)      ? 0 : c[pos - 1];
    int next = (pos == sz - 1) ? 0 : c[pos + 1];
    return dist(prev, next) - dist(prev, c[pos]) - dist(c[pos], next);
}

/**
 * @brief Перебирает все ходы Relocate, возвращает лучший допустимый.
 *
 * Допустимость: не в tabu (или критерий аспирации выполнен).
 */
RelocateMove best_relocate(const Solution& sol, const Instance& inst,
                           const DistanceMatrix& dist,
                           const std::vector<TabuEntry>& tabu_list,
                           double best_known, double penalty,
                           int cur_iter) {
    const int m = static_cast<int>(sol.routes.size());
    RelocateMove best{-1, -1, -1, -1, -1,
                      std::numeric_limits<double>::infinity()};

    double cur_eval = eval(sol, dist, penalty);

    for (int r1 = 0; r1 < m; ++r1) {
        if (sol.routes[r1].empty()) continue;
        const auto& c1 = sol.routes[r1].clients;

        for (int p1 = 0; p1 < static_cast<int>(c1.size()); ++p1) {
            int cli    = c1[p1];
            int demand = inst.clients[cli].demand;
            double rem = removal_cost(sol.routes[r1], p1, dist);

            // Изменение num_routes после удаления из r1:
            // если r1 становится пустым — -1.
            int delta_routes_src = (sol.routes[r1].size() == 1) ? -1 : 0;

            for (int r2 = 0; r2 < m; ++r2) {
                if (r2 == r1) continue;
                if (sol.routes[r2].demand(inst) + demand > inst.capacity) continue;

                // Лучшая позиция вставки.
                const auto& c2  = sol.routes[r2].clients;
                const int   sz2 = static_cast<int>(c2.size());
                double best_ins = std::numeric_limits<double>::infinity();
                int    best_pos = 0;
                for (int ins = 0; ins <= sz2; ++ins) {
                    double ic = insertion_cost(sol.routes[r2], cli, ins, dist);
                    if (ic < best_ins) { best_ins = ic; best_pos = ins; }
                }

                double delta_len    = rem + best_ins;
                double delta_routes = static_cast<double>(delta_routes_src);
                double delta_cost   = delta_len + penalty * delta_routes;

                // Проверка запрета и аспирации.
                bool tabu = is_tabu(tabu_list, cli, r1, cur_iter);
                bool aspiration = (cur_eval + delta_cost < best_known - 1e-9);

                if (!tabu || aspiration) {
                    if (delta_cost < best.delta) {
                        best = {r1, p1, cli, r2, best_pos, delta_cost};
                    }
                }
            }
        }
    }
    return best;
}

/**
 * @brief Перебирает все ходы Swap, возвращает лучший допустимый.
 */
SwapMove best_swap(const Solution& sol, const Instance& inst,
                   const DistanceMatrix& dist,
                   const std::vector<TabuEntry>& tabu_list,
                   double best_known, double penalty,
                   int cur_iter) {
    const int m = static_cast<int>(sol.routes.size());
    SwapMove best{-1, -1, -1, -1, -1, -1,
                  std::numeric_limits<double>::infinity()};

    double cur_eval = eval(sol, dist, penalty);

    for (int r1 = 0; r1 < m; ++r1) {
        if (sol.routes[r1].empty()) continue;
        const auto& c1 = sol.routes[r1].clients;
        int ld1 = sol.routes[r1].demand(inst);

        for (int p1 = 0; p1 < static_cast<int>(c1.size()); ++p1) {
            int ci = c1[p1];
            int di = inst.clients[ci].demand;
            double rem_ci = removal_cost(sol.routes[r1], p1, dist);

            for (int r2 = r1 + 1; r2 < m; ++r2) {
                if (sol.routes[r2].empty()) continue;
                const auto& c2 = sol.routes[r2].clients;
                int ld2 = sol.routes[r2].demand(inst);

                for (int p2 = 0; p2 < static_cast<int>(c2.size()); ++p2) {
                    int cj = c2[p2];
                    int dj = inst.clients[cj].demand;

                    if (ld1 - di + dj > inst.capacity) continue;
                    if (ld2 - dj + di > inst.capacity) continue;

                    double rem_cj = removal_cost(sol.routes[r2], p2, dist);

                    // Вставка cj на место ci в r1 (та же позиция).
                    int pv1 = (p1 == 0) ? 0 : c1[p1 - 1];
                    int nx1 = (p1 == static_cast<int>(c1.size())-1) ? 0 : c1[p1+1];
                    double ins_cj_r1 = dist(pv1,cj)+dist(cj,nx1)-dist(pv1,nx1);

                    // Вставка ci на место cj в r2.
                    int pv2 = (p2 == 0) ? 0 : c2[p2 - 1];
                    int nx2 = (p2 == static_cast<int>(c2.size())-1) ? 0 : c2[p2+1];
                    double ins_ci_r2 = dist(pv2,ci)+dist(ci,nx2)-dist(pv2,nx2);

                    double delta_cost = rem_ci + ins_cj_r1 + rem_cj + ins_ci_r2;

                    bool tabu1 = is_tabu(tabu_list, ci, r1, cur_iter);
                    bool tabu2 = is_tabu(tabu_list, cj, r2, cur_iter);
                    bool aspiration = (cur_eval + delta_cost < best_known - 1e-9);

                    if ((!tabu1 && !tabu2) || aspiration) {
                        if (delta_cost < best.delta) {
                            best = {r1, p1, ci, r2, p2, cj, delta_cost};
                        }
                    }
                }
            }
        }
    }
    return best;
}

/**
 * @brief Удаляет пустые маршруты из решения.
 */
void remove_empty(Solution& sol) {
    sol.routes.erase(
        std::remove_if(sol.routes.begin(), sol.routes.end(),
                       [](const Route& r){ return r.empty(); }),
        sol.routes.end());
}

} // namespace

// ─── TabuSearch::optimize ──────────────────────────────────────────────────────

void TabuSearch::optimize(Solution& sol, const Instance& inst,
                          const DistanceMatrix& dist, const TSParams& params) {
    // Применяем 2-opt как начальное улучшение.
    TwoOpt::improve(sol, inst, dist);

    Solution best_sol  = sol;
    double   best_cost = eval(sol, dist, params.vehicle_penalty);

    std::vector<TabuEntry> tabu_list;
    tabu_list.reserve(static_cast<size_t>(params.tabu_tenure) * 4);

    int no_improve = 0;

    for (int iter = 0; iter < params.max_iter && no_improve < params.max_no_improve; ++iter) {
        // Находим лучший ход Relocate и лучший ход Swap.
        RelocateMove rm = best_relocate(sol, inst, dist, tabu_list,
                                        best_cost, params.vehicle_penalty, iter);
        SwapMove     sm = best_swap(sol, inst, dist, tabu_list,
                                    best_cost, params.vehicle_penalty, iter);

        // Выбираем лучший из двух типов ходов.
        bool do_relocate = (rm.r1 != -1)
            && (sm.r1 == -1 || rm.delta <= sm.delta);
        bool do_swap = !do_relocate && (sm.r1 != -1);

        if (!do_relocate && !do_swap) {
            // Нет допустимых ходов.
            ++no_improve;
            continue;
        }

        if (do_relocate) {
            // Выполняем Relocate.
            int client = sol.routes[rm.r1].clients[rm.pos1];
            sol.routes[rm.r1].clients.erase(
                sol.routes[rm.r1].clients.begin() + rm.pos1);
            sol.routes[rm.r2].clients.insert(
                sol.routes[rm.r2].clients.begin() + rm.ins_pos, client);

            // Запрещаем возврат client в r1.
            add_tabu(tabu_list, client, rm.r1, iter, params.tabu_tenure);
        } else {
            // Выполняем Swap.
            std::swap(sol.routes[sm.r1].clients[sm.p1],
                      sol.routes[sm.r2].clients[sm.p2]);

            add_tabu(tabu_list, sm.c1, sm.r1, iter, params.tabu_tenure);
            add_tabu(tabu_list, sm.c2, sm.r2, iter, params.tabu_tenure);
        }

        remove_empty(sol);

        // Применяем быстрый 2-opt к изменённым маршрутам.
        TwoOpt::improve(sol, inst, dist);

        double cur_cost = eval(sol, dist, params.vehicle_penalty);

        if (cur_cost < best_cost - 1e-9) {
            best_sol   = sol;
            best_cost  = cur_cost;
            no_improve = 0;
        } else {
            ++no_improve;
        }
    }

    sol = std::move(best_sol);
}

} // namespace vrp
