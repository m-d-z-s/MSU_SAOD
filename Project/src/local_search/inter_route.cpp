#include "local_search/inter_route.hpp"

#include <vector>
#include <limits>
#include <algorithm>

namespace vrp {

namespace {

/**
 * @brief Вычисляет стоимость удаления клиента по индексу pos из маршрута.
 *
 * Удаление клиента c[pos] из маршрута [... → prev → c[pos] → next → ...]:
 *   cost = dist(prev, next) - dist(prev, c[pos]) - dist(c[pos], next)
 * (отрицательное значение = уменьшение длины маршрута).
 *
 * @param route Маршрут.
 * @param pos   Индекс клиента в route.clients.
 * @param dist  Матрица расстояний.
 * @return Изменение длины маршрута при удалении (≤ 0 обычно).
 */
double cost_of_removal(const Route& route, int pos, const DistanceMatrix& dist) {
    const auto& c  = route.clients;
    const int   sz = static_cast<int>(c.size());

    int prev = (pos == 0)      ? 0 : c[pos - 1];
    int next = (pos == sz - 1) ? 0 : c[pos + 1];

    return dist(prev, next) - dist(prev, c[pos]) - dist(c[pos], next);
}

/**
 * @brief Вычисляет наилучшую стоимость вставки клиента client в маршрут.
 *
 * Перебирает все позиции вставки (до каждого существующего клиента и в конец).
 *
 * @param route  Маршрут-приёмник.
 * @param client Индекс вставляемого клиента.
 * @param dist   Матрица расстояний.
 * @param best_pos Выходной параметр: индекс в route.clients ПЕРЕД которым вставляем.
 *                 sz означает «вставить в конец».
 * @return Минимальная стоимость вставки (изменение длины, ≤ 0 если выгодно).
 */
double best_insertion_cost(const Route& route, int client,
                           const DistanceMatrix& dist, int& best_pos) {
    const auto& c  = route.clients;
    const int   sz = static_cast<int>(c.size());

    double best_cost = std::numeric_limits<double>::infinity();
    best_pos = 0;

    // Вставка ПЕРЕД c[ins] (ins от 0 до sz включительно — последнее = вставить в конец).
    for (int ins = 0; ins <= sz; ++ins) {
        int before = (ins == 0)  ? 0 : c[ins - 1];
        int after  = (ins == sz) ? 0 : c[ins];

        double cost = dist(before, client) + dist(client, after)
                    - dist(before, after);

        if (cost < best_cost) {
            best_cost = cost;
            best_pos  = ins;
        }
    }

    return best_cost;
}

/**
 * @brief Удаляет пустые маршруты из решения.
 */
void remove_empty_routes(Solution& sol) {
    sol.routes.erase(
        std::remove_if(sol.routes.begin(), sol.routes.end(),
                       [](const Route& r) { return r.empty(); }),
        sol.routes.end());
}

} // namespace

// ─── InterRoute::relocate ─────────────────────────────────────────────────────

double InterRoute::relocate(Solution& sol, const Instance& inst,
                            const DistanceMatrix& dist) {
    double total_gain = 0.0;
    bool   improved   = true;

    while (improved) {
        improved = false;

        const int m = static_cast<int>(sol.routes.size());

        double best_delta  = -1e-9;
        int    best_r1     = -1; // маршрут-источник
        int    best_pos1   = -1; // позиция клиента в r1
        int    best_r2     = -1; // маршрут-приёмник
        int    best_ins    = -1; // позиция вставки в r2

        for (int r1 = 0; r1 < m; ++r1) {
            if (sol.routes[r1].empty()) continue;
            const auto& c1 = sol.routes[r1].clients;

            for (int pos1 = 0; pos1 < static_cast<int>(c1.size()); ++pos1) {
                int client = c1[pos1];
                int demand = inst.clients[client].demand;

                double remove_cost = cost_of_removal(sol.routes[r1], pos1, dist);

                for (int r2 = 0; r2 < m; ++r2) {
                    if (r2 == r1) continue;

                    // Проверяем вместимость маршрута r2 после вставки.
                    int load_r2 = sol.routes[r2].demand(inst);
                    if (load_r2 + demand > inst.capacity) continue;

                    int    ins_pos;
                    double insert_cost = best_insertion_cost(
                        sol.routes[r2], client, dist, ins_pos);

                    double delta = remove_cost + insert_cost;

                    if (delta < best_delta) {
                        best_delta = delta;
                        best_r1    = r1;
                        best_pos1  = pos1;
                        best_r2    = r2;
                        best_ins   = ins_pos;
                    }
                }
            }
        }

        if (best_r1 == -1) break;

        // Выполняем перенос.
        int client = sol.routes[best_r1].clients[best_pos1];
        sol.routes[best_r1].clients.erase(
            sol.routes[best_r1].clients.begin() + best_pos1);
        sol.routes[best_r2].clients.insert(
            sol.routes[best_r2].clients.begin() + best_ins, client);

        total_gain -= best_delta;
        improved    = true;
    }

    remove_empty_routes(sol);
    return total_gain;
}

// ─── InterRoute::swap ─────────────────────────────────────────────────────────

double InterRoute::swap(Solution& sol, const Instance& inst,
                        const DistanceMatrix& dist) {
    double total_gain = 0.0;
    bool   improved   = true;

    while (improved) {
        improved = false;

        const int m = static_cast<int>(sol.routes.size());

        double best_delta = -1e-9;
        int    best_r1 = -1, best_p1 = -1;
        int    best_r2 = -1, best_p2 = -1;

        for (int r1 = 0; r1 < m; ++r1) {
            if (sol.routes[r1].empty()) continue;
            auto& c1 = sol.routes[r1].clients;

            for (int p1 = 0; p1 < static_cast<int>(c1.size()); ++p1) {
                int ci  = c1[p1];
                int di  = inst.clients[ci].demand;
                int ld1 = sol.routes[r1].demand(inst); // нагрузка r1

                double rem_ci = cost_of_removal(sol.routes[r1], p1, dist);

                for (int r2 = r1 + 1; r2 < m; ++r2) {
                    if (sol.routes[r2].empty()) continue;
                    auto& c2 = sol.routes[r2].clients;

                    int ld2 = sol.routes[r2].demand(inst); // нагрузка r2

                    for (int p2 = 0; p2 < static_cast<int>(c2.size()); ++p2) {
                        int cj = c2[p2];
                        int dj = inst.clients[cj].demand;

                        // Проверяем вместимость после обмена:
                        // r1: ld1 - di + dj <= capacity
                        // r2: ld2 - dj + di <= capacity
                        if (ld1 - di + dj > inst.capacity) continue;
                        if (ld2 - dj + di > inst.capacity) continue;

                        double rem_cj = cost_of_removal(sol.routes[r2], p2, dist);

                        // Стоимость вставки cj на место ci в r1:
                        // временно удаляем ci, считаем вставку cj, восстанавливаем.
                        // Упрощение: вставляем cj на позицию p1 (та же позиция).
                        int prev1 = (p1 == 0) ? 0 : c1[p1 - 1];
                        int next1 = (p1 == static_cast<int>(c1.size()) - 1) ? 0 : c1[p1 + 1];
                        double ins_cj_r1 = dist(prev1, cj) + dist(cj, next1)
                                         - dist(prev1, next1);

                        int prev2 = (p2 == 0) ? 0 : c2[p2 - 1];
                        int next2 = (p2 == static_cast<int>(c2.size()) - 1) ? 0 : c2[p2 + 1];
                        double ins_ci_r2 = dist(prev2, ci) + dist(ci, next2)
                                         - dist(prev2, next2);

                        double delta = rem_ci + ins_cj_r1 + rem_cj + ins_ci_r2;

                        if (delta < best_delta) {
                            best_delta = delta;
                            best_r1 = r1; best_p1 = p1;
                            best_r2 = r2; best_p2 = p2;
                        }
                    }
                }
            }
        }

        if (best_r1 == -1) break;

        // Выполняем обмен.
        std::swap(sol.routes[best_r1].clients[best_p1],
                  sol.routes[best_r2].clients[best_p2]);

        total_gain -= best_delta;
        improved    = true;
    }

    return total_gain;
}

// ─── InterRoute::improve ─────────────────────────────────────────────────────

double InterRoute::improve(Solution& sol, const Instance& inst,
                           const DistanceMatrix& dist) {
    double total_gain = 0.0;
    bool   improved   = true;

    while (improved) {
        improved = false;

        double g = relocate(sol, inst, dist);
        if (g > 1e-9) { total_gain += g; improved = true; }

        g = swap(sol, inst, dist);
        if (g > 1e-9) { total_gain += g; improved = true; }
    }

    return total_gain;
}

} // namespace vrp
