#include "distance.hpp"

namespace vrp {

DistanceMatrix::DistanceMatrix(const Instance& inst)
    : n_(static_cast<int>(inst.clients.size()))
    , data_(n_ * n_)
{
    for (int i = 0; i < n_; ++i) {
        for (int j = 0; j < n_; ++j) {
            double dx = inst.clients[i].x - inst.clients[j].x;
            double dy = inst.clients[i].y - inst.clients[j].y;
            data_[i * n_ + j] = std::sqrt(dx * dx + dy * dy);
        }
    }
}

} // namespace vrp
