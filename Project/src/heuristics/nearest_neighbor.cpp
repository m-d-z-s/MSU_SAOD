#include "nearest_neighbor.hpp"

#include <vector>
#include <limits>

namespace vrp {

Solution NearestNeighbor::solve(const Instance& inst, const DistanceMatrix& dist) {
    const int n = inst.num_clients(); // клиенты с индексами 1..n
    std::vector<bool> visited(n + 1, false);
    int remaining = n;

    Solution sol;

    while (remaining > 0) {
        Route route;
        int load = 0;
        int current = 0; // начинаем из депо (индекс 0)

        // Строим маршрут жадно: берём ближайшего подходящего клиента.
        bool added = true;
        while (added) {
            added = false;
            int    best_client = -1;
            double best_dist   = std::numeric_limits<double>::infinity();

            for (int c = 1; c <= n; ++c) {
                if (visited[c]) continue;
                if (load + inst.clients[c].demand > inst.capacity) continue;
                double d = dist(current, c);
                if (d < best_dist) {
                    best_dist   = d;
                    best_client = c;
                }
            }

            if (best_client != -1) {
                route.clients.push_back(best_client);
                load    += inst.clients[best_client].demand;
                visited[best_client] = true;
                current  = best_client;
                --remaining;
                added    = true;
            }
        }

        // Маршрут должен содержать хотя бы одного клиента.
        // Если ни один не добавился — инстанс некорректен (клиент с demand > capacity).
        if (!route.empty()) {
            sol.routes.push_back(std::move(route));
        } else {
            // Аварийный выход: добавляем первого непосещённого отдельным маршрутом,
            // чтобы не зациклиться (нарушение вместимости в инстансе).
            for (int c = 1; c <= n; ++c) {
                if (!visited[c]) {
                    Route r;
                    r.clients.push_back(c);
                    visited[c] = true;
                    --remaining;
                    sol.routes.push_back(std::move(r));
                    break;
                }
            }
        }
    }

    return sol;
}

} // namespace vrp
