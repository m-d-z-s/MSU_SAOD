#include "local_search/or_opt.hpp"

#include <vector>
#include <limits>

namespace vrp {

// ─── OrOpt::improve_route ─────────────────────────────────────────────────────

/**
 * @brief Один проход Or-opt для сегментов длины k внутри маршрута.
 *
 * Алгоритм:
 *   Для каждого возможного сегмента [seg_start, seg_start+k-1]:
 *     Вычисляем стоимость удаления сегмента из текущей позиции.
 *     Для каждой позиции вставки (не перекрывающейся с сегментом):
 *       Вычисляем delta = cost_remove + cost_insert.
 *       Запоминаем лучший ход (best improvement).
 *   Применяем лучший ход, повторяем до отсутствия улучшений.
 */
double OrOpt::improve_route(Route& route, const DistanceMatrix& dist, int k) {
    if (route.size() < static_cast<int>(k) + 1) return 0.0;

    double total_gain = 0.0;
    bool   improved   = true;
    auto&  c          = route.clients;

    while (improved) {
        improved = false;

        double best_delta = -1e-9; // принимаем только строгое улучшение
        int    best_seg   = -1;    // начало лучшего сегмента в c[]
        int    best_ins   = -1;    // позиция вставки: вставить ПОСЛЕ c[best_ins]
                                   // (best_ins == -1 означает «в начало маршрута»)

        const int sz = static_cast<int>(c.size());

        for (int seg = 0; seg <= sz - k; ++seg) {
            // Сегмент: c[seg], c[seg+1], ..., c[seg+k-1]
            int seg_first = c[seg];
            int seg_last  = c[seg + k - 1];

            // Узлы до и после сегмента в маршруте (с учётом депо).
            int prev_node = (seg == 0)       ? 0 : c[seg - 1];
            int next_node = (seg + k == sz)  ? 0 : c[seg + k];

            // Стоимость удаления сегмента:
            // убираем рёбра (prev→seg_first) и (seg_last→next),
            // добавляем ребро (prev→next).
            double cost_remove = dist(prev_node, next_node)
                               - dist(prev_node, seg_first)
                               - dist(seg_last,  next_node);

            // Перебираем позиции вставки: вставить сегмент ПОСЛЕ позиции ins.
            // ins == -1: вставить перед c[0] (после депо).
            // ins == j: вставить между c[j] и c[j+1] (или депо если j == sz-1).
            // Нельзя вставлять внутрь самого сегмента.
            for (int ins = -1; ins < sz; ++ins) {
                // Пропускаем позиции внутри или вплотную к сегменту (не дают изменений).
                if (ins >= seg - 1 && ins < seg + k) continue;

                int ins_before = (ins == -1)     ? 0 : c[ins];
                int ins_after  = (ins + 1 == sz) ? 0 : c[ins + 1];
                // Но если ins + 1 попадает в сегмент, скорректируем:
                // это уже отфильтровано условием выше.

                // Стоимость вставки сегмента между ins_before и ins_after:
                // убираем ребро (ins_before→ins_after),
                // добавляем (ins_before→seg_first) и (seg_last→ins_after).
                double cost_insert = dist(ins_before, seg_first)
                                   + dist(seg_last,  ins_after)
                                   - dist(ins_before, ins_after);

                double delta = cost_remove + cost_insert;

                if (delta < best_delta) {
                    best_delta = delta;
                    best_seg   = seg;
                    best_ins   = ins;
                }
            }
        }

        if (best_seg == -1) break; // улучшений нет

        // Применяем лучший ход: извлекаем сегмент и вставляем в новую позицию
        std::vector<int> seg_clients(c.begin() + best_seg,
                                     c.begin() + best_seg + k);

        // Удаляем сегмент
        c.erase(c.begin() + best_seg, c.begin() + best_seg + k);

        // Пересчитываем позицию вставки после удаления.
        // Если best_ins >= best_seg + k, позиция сдвинулась влево на k.
        // Если best_ins < best_seg, позиция не изменилась.
        int ins_pos; // вставить ПЕРЕД c[ins_pos] (или в конец если ins_pos == sz-k)
        if (best_ins < best_seg) {
            ins_pos = best_ins + 1; // вставить после best_ins → перед best_ins+1
        } else {
            // best_ins >= best_seg + k (отфильтровано выше)
            ins_pos = best_ins + 1 - k;
        }

        c.insert(c.begin() + ins_pos, seg_clients.begin(), seg_clients.end());

        total_gain -= best_delta;
        improved    = true;
    }

    return total_gain;
}

// ─── OrOpt::improve ───────────────────────────────────────────────────────────

double OrOpt::improve(Solution& sol, const Instance& /*inst*/,
                      const DistanceMatrix& dist) {
    double total_gain = 0.0;
    for (Route& r : sol.routes) {
        // Применяем Or-opt для k=1, 2, 3 в порядке убывания — более широкие
        // ходы первыми дают более крупные улучшения.
        for (int k = 1; k <= 3; ++k) {
            total_gain += improve_route(r, dist, k);
        }
    }
    return total_gain;
}

} // namespace vrp
