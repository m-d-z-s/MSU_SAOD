#include "vrp_parser.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>

namespace vrp {

// Убирает пробелы по краям строки.
static std::string trim(const std::string& s) {
    size_t lo = s.find_first_not_of(" \t\r\n");
    if (lo == std::string::npos) return {};
    size_t hi = s.find_last_not_of(" \t\r\n");
    return s.substr(lo, hi - lo + 1);
}

// Читает значение после двоеточия в строке вида "KEY : VALUE".
static std::string after_colon(const std::string& line) {
    size_t pos = line.find(':');
    if (pos == std::string::npos) return trim(line);
    return trim(line.substr(pos + 1));
}

Instance VrpParser::parse(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("Cannot open file: " + path);
    }

    Instance inst;
    inst.capacity     = 0;
    inst.num_vehicles = 0;

    int dimension = 0;

    // Секции файла: читаем построчно, переключаем режим по ключевым словам.
    enum class Section { None, NodeCoord, Demand, Depot };
    Section section = Section::None;

    // Временные хранилища — заполняем координаты и спрос отдельно,
    // потом соединяем по индексу.
    std::vector<double> xs, ys;
    std::vector<int>    demands;

    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty()) continue;

        // Ключевые слова секций (без двоеточия).
        if (line == "NODE_COORD_SECTION") { section = Section::NodeCoord; continue; }
        if (line == "DEMAND_SECTION")     { section = Section::Demand;    continue; }
        if (line == "DEPOT_SECTION")      { section = Section::Depot;     continue; }
        if (line == "EOF")                { break; }

        // Строки с двоеточием — заголовки.
        if (line.find(':') != std::string::npos) {
            section = Section::None;
            std::string key = trim(line.substr(0, line.find(':')));
            std::string val = after_colon(line);

            // Переводим ключ в верхний регистр для надёжного сравнения.
            std::transform(key.begin(), key.end(), key.begin(), ::toupper);

            if (key == "NAME")      { inst.name    = val; }
            else if (key == "DIMENSION") { dimension = std::stoi(val); }
            else if (key == "CAPACITY")  { inst.capacity = std::stoi(val); }
            // Остальные заголовки (COMMENT, TYPE, ...) игнорируем.
            continue;
        }

        // Данные секций.
        std::istringstream ss(line);

        if (section == Section::NodeCoord) {
            int id; double x, y;
            if (ss >> id >> x >> y) {
                xs.push_back(x);
                ys.push_back(y);
            }
        } else if (section == Section::Demand) {
            int id, demand;
            if (ss >> id >> demand) {
                demands.push_back(demand);
            }
        }
        // DEPOT_SECTION — просто пропускаем (депо всегда = узел 1 = index 0).
    }

    // Проверяем согласованность данных.
    if (dimension == 0) {
        throw std::runtime_error("DIMENSION not found in " + path);
    }
    if (static_cast<int>(xs.size()) != dimension) {
        throw std::runtime_error("NODE_COORD_SECTION size mismatch in " + path);
    }
    if (static_cast<int>(demands.size()) != dimension) {
        throw std::runtime_error("DEMAND_SECTION size mismatch in " + path);
    }

    // Заполняем инстанс: узел 0 = депо (первая строка TSPLIB).
    inst.clients.resize(dimension);
    for (int i = 0; i < dimension; ++i) {
        inst.clients[i] = Client{i, xs[i], ys[i], demands[i], 0, 0, 0};
    }

    return inst;
}

} // namespace vrp
