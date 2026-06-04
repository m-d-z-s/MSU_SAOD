#include "clarke_wright.hpp"

#include <algorithm>
#include <vector>

namespace vrp {

namespace {

/**
 * @brief Одна запись в списке экономий.
 */
struct Saving {
    double value; ///< s(i,j) = dist[0][i] + dist[0][j] - dist[i][j]
    int    i;     ///< Индекс первого клиента
    int    j;     ///< Индекс второго клиента
};

} // namespace

/**
 * @brief Реализация параллельного алгоритма Кларка-Райта.
 *
 * Для отслеживания, к какому маршруту принадлежит клиент и является ли
 * он крайним (первым/последним), используем вспомогательные массивы:
 *   route_of[c]    — индекс маршрута, в котором находится клиент c
 *   load[r]        — суммарный спрос маршрута r
 *
 * Клиент c является «хвостом» маршрута r, если routes[r].clients.back() == c.
 * Клиент c является «головой» маршрута r, если routes[r].clients.front() == c.
 *
 * При слиянии: конец маршрута A -> начало маршрута B.
 * Маршрут B переносится в хвост A, маршрут B помечается пустым.
 */
Solution ClarkeWright::solve(const Instance& inst, const DistanceMatrix& dist) {
    const int n = inst.num_clients();

    // Стартовое решение: маршрут r соответствует клиенту (r+1).
    // routes[r].clients = {r+1}, route_of[r+1] = r.
    std::vector<Route> routes(n);
    std::vector<int>   load(n);
    std::vector<int>   route_of(n + 1); // route_of[c] -> индекс маршрута

    for (int i = 0; i < n; ++i) {
        routes[i].clients = {i + 1};
        load[i]           = inst.clients[i + 1].demand;
        route_of[i + 1]   = i;
    }

    // Вычисляет все экономии
    std::vector<Saving> savings;
    savings.reserve(static_cast<size_t>(n) * (n - 1) / 2);
    for (int i = 1; i <= n; ++i) {
        for (int j = i + 1; j <= n; ++j) {
            double s = dist(0, i) + dist(0, j) - dist(i, j);
            savings.push_back({s, i, j});
        }
    }

    // Сортирует по убыванию экономии
    std::sort(savings.begin(), savings.end(),
              [](const Saving& a, const Saving& b) { return a.value > b.value; });

    // Жадное слияние маршрутов
    for (const Saving& sv : savings) {
        int ci = sv.i;
        int cj = sv.j;

        int ri = route_of[ci];
        int rj = route_of[cj];

        // Клиенты должны быть в разных непустых маршрутах
        if (ri == rj) continue;
        if (routes[ri].empty() || routes[rj].empty()) continue;

        // ci должен быть хвостом маршрута ri, cj — головой маршрута rj.
        // Если не так — проверяем симметричный вариант: cj-хвост ri, ci-голова rj
        bool ci_tail_ri = (routes[ri].clients.back()  == ci);
        bool cj_head_rj = (routes[rj].clients.front() == cj);
        bool cj_tail_rj = (routes[rj].clients.back()  == cj);
        bool ci_head_ri = (routes[ri].clients.front() == ci);

        bool can_merge_ij = ci_tail_ri && cj_head_rj;
        bool can_merge_ji = cj_tail_rj && ci_head_ri;

        if (!can_merge_ij && !can_merge_ji) continue;

        // Проверяет вместимость
        if (load[ri] + load[rj] > inst.capacity) continue;

        // Выполняет слияние: хвост A + голова B
        if (can_merge_ij) {
            // Присоединяет маршрут rj к концу ri
            for (int c : routes[rj].clients) {
                routes[ri].clients.push_back(c);
                route_of[c] = ri;
            }
        } else {
            // can_merge_ji: присоединяет маршрут ri к концу rj
            for (int c : routes[ri].clients) {
                routes[rj].clients.push_back(c);
                route_of[c] = rj;
            }
            // Переносит результат в ri, очищает rj
            routes[ri] = std::move(routes[rj]);
            // Обновляет route_of для всех клиентов нового ri
            for (int c : routes[ri].clients) {
                route_of[c] = ri;
            }
        }

        load[ri] += load[rj];
        routes[rj].clients.clear();
        load[rj] = 0;
    }

    // Собирает непустые маршруты в решение
    Solution sol;
    for (Route& r : routes) {
        if (!r.empty()) {
            sol.routes.push_back(std::move(r));
        }
    }

    return sol;
}

} // namespace vrp
