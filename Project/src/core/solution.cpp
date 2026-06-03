#include "solution.hpp"
#include <sstream>
#include <vector>

namespace vrp {

// ─── Route ───────────────────────────────────────────────────────────────────

int Route::demand(const Instance& inst) const {
    int total = 0;
    for (int c : clients) {
        total += inst.clients[c].demand;
    }
    return total;
}

double Route::length(const DistanceMatrix& dist) const {
    if (clients.empty()) return 0.0;

    double len = dist(0, clients.front())   // депо → первый клиент
               + dist(clients.back(), 0);   // последний клиент → депо

    for (int i = 0; i + 1 < static_cast<int>(clients.size()); ++i) {
        len += dist(clients[i], clients[i + 1]);
    }
    return len;
}

// ─── Solution ────────────────────────────────────────────────────────────────

double Solution::total_length(const DistanceMatrix& dist) const {
    double total = 0.0;
    for (const Route& r : routes) {
        total += r.length(dist);
    }
    return total;
}

int Solution::num_routes() const {
    int count = 0;
    for (const Route& r : routes) {
        if (!r.empty()) ++count;
    }
    return count;
}

std::string Solution::validate(const Instance& inst) const {
    const int n = inst.num_clients();

    // Считаем сколько раз каждый клиент встречается
    std::vector<int> visits(n + 1, 0); // индексы 1..n

    for (const Route& r : routes) {
        int load = 0;
        for (int c : r.clients) {
            if (c < 1 || c > n) {
                std::ostringstream os;
                os << "invalid client index " << c;
                return os.str();
            }
            ++visits[c];
            load += inst.clients[c].demand;
        }
        if (load > inst.capacity) {
            std::ostringstream os;
            os << "capacity exceeded: load=" << load
               << " capacity=" << inst.capacity;
            return os.str();
        }
    }

    for (int i = 1; i <= n; ++i) {
        if (visits[i] != 1) {
            std::ostringstream os;
            os << "client " << i << " visited " << visits[i] << " times";
            return os.str();
        }
    }

    return {}; // OK
}

} // namespace vrp
