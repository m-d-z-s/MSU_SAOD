#include "local_search/two_opt.hpp"

#include <algorithm>
#include <vector>

namespace vrp {

// ─── TwoOpt::improve_route ────────────────────────────────────────────────────

double TwoOpt::improve_route(Route& route, const DistanceMatrix& dist) {
    // Маршрут с 0 или 1 клиентом не улучшается.
    if (route.size() < 2) return 0.0;

    double total_gain = 0.0;
    bool   improved   = true;

    auto& c = route.clients; // псевдоним для читаемости
    const int sz = route.size();

    while (improved) {
        improved = false;

        // Перебираем все пары рёбер (i, i+1) и (j, j+1).
        // Ребро (i, i+1): от c[i] к c[i+1], или от депо к c[0], или от c[sz-1] к депо.
        // Индексируем с учётом депо на концах:
        //   prev(i) = (i == 0)    ? 0 : c[i-1]
        //   next(j) = (j == sz-1) ? 0 : c[j+1]
        //
        // Переворачиваем сегмент [i..j]: индексы в clients-массиве.

        for (int i = 0; i < sz - 1 && !improved; ++i) {
            // Узел перед сегментом (в маршруте с депо по краям).
            int node_before_i = (i == 0) ? 0 : c[i - 1];

            for (int j = i + 1; j < sz; ++j) {
                // Узел после сегмента.
                int node_after_j = (j == sz - 1) ? 0 : c[j + 1];

                // Текущие рёбра: (node_before_i → c[i]) и (c[j] → node_after_j).
                // После переворота: (node_before_i → c[j]) и (c[i] → node_after_j).
                double delta = dist(node_before_i, c[j])
                             + dist(c[i],          node_after_j)
                             - dist(node_before_i, c[i])
                             - dist(c[j],          node_after_j);

                if (delta < -1e-9) {
                    // Переворачиваем сегмент [i..j].
                    std::reverse(c.begin() + i, c.begin() + j + 1);
                    total_gain -= delta;
                    improved    = true;
                    break; // first improvement: начинаем заново
                }
            }
        }
    }

    return total_gain;
}

// ─── TwoOpt::improve ─────────────────────────────────────────────────────────

double TwoOpt::improve(Solution& sol, const Instance& /*inst*/,
                       const DistanceMatrix& dist) {
    double total_gain = 0.0;
    for (Route& r : sol.routes) {
        total_gain += improve_route(r, dist);
    }
    return total_gain;
}

} // namespace vrp
