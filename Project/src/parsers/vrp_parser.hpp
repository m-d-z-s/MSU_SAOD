#pragma once

#include "core/instance.hpp"
#include <string>

namespace vrp {

/**
 * @brief Парсер формата TSPLIB (.vrp) для CVRP.
 *
 * Поддерживает секции: NAME, DIMENSION, CAPACITY,
 * NODE_COORD_SECTION, DEMAND_SECTION, DEPOT_SECTION.
 * Тип весов ребёр — только EUC_2D.
 */
class VrpParser {
public:
    /**
     * @brief Читает инстанс из файла в формате TSPLIB.
     * @param path Путь к файлу .vrp.
     * @return Загруженный инстанс.
     * @throws std::runtime_error при ошибке чтения или неверном формате.
     */
    static Instance parse(const std::string& path);
};

} // namespace vrp
