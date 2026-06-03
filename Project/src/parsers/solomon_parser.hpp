#pragma once

#include "core/instance.hpp"
#include <string>

namespace vrp {

/**
 * @brief Парсер формата Solomon для VRPTW.
 *
 * Формат файла:
 *   Строка 1: название инстанса
 *   Строка 4: число машин и вместимость
 *   Строки 9+: id x y demand ready due service
 *
 * Индекс 0 в clients[] — депо (первая строка данных).
 */
class SolomonParser {
public:
    /**
     * @brief Читает инстанс из файла в формате Solomon.
     * @param path Путь к файлу .txt.
     * @return Загруженный инстанс с временными окнами.
     * @throws std::runtime_error при ошибке чтения.
     */
    static Instance parse(const std::string& path);
};

} // namespace vrp
