#pragma once

#include "instance.hpp"
#include <vector>
#include <cmath>

namespace vrp {

/**
 * @brief Предвычисленная матрица расстояний (flat, row-major).
 *
 * Евклидово расстояние между всеми парами узлов вычисляется один раз.
 * Обращение: dist(i, j) — O(1), без повторных sqrt().
 *
 * Память: O(n²), где n = число узлов (включая депо).
 */
class DistanceMatrix {
public:
    /**
     * @brief Строит матрицу по инстансу.
     * @param inst Инстанс задачи.
     */
    explicit DistanceMatrix(const Instance& inst);

    /**
     * @brief Возвращает расстояние между узлами i и j.
     * @param i Индекс первого узла.
     * @param j Индекс второго узла.
     * @return Евклидово расстояние.
     */
    double operator()(int i, int j) const { return data_[i * n_ + j]; }

    /** @brief Число узлов (включая депо). */
    int size() const { return n_; }

private:
    int                 n_;
    std::vector<double> data_;
};

} // namespace vrp
