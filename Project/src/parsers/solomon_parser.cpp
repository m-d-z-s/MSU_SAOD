#include "solomon_parser.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace vrp {

namespace {

// Возвращает true, если строка не пустая и начинается с цифры.
bool starts_with_digit(const std::string& line) {
    size_t pos = line.find_first_not_of(" \t\r\n");
    return pos != std::string::npos && std::isdigit(static_cast<unsigned char>(line[pos]));
}

// Убирает пробелы по краям.
std::string trim(const std::string& s) {
    size_t lo = s.find_first_not_of(" \t\r\n");
    if (lo == std::string::npos) return {};
    size_t hi = s.find_last_not_of(" \t\r\n");
    return s.substr(lo, hi - lo + 1);
}

} // namespace

/**
 * @brief Формат файла Solomon:
 *
 *   Строка 1: название инстанса
 *   Строки 2-3: пустые/служебные
 *   Строки 4-5: заголовок VEHICLE и "NUMBER CAPACITY"
 *   Строка 6: значения числа машин и вместимости
 *   Строки 7-9: заголовок CUSTOMER и названия столбцов
 *   Строки 10+: данные клиентов (id x y demand ready due service)
 */
Instance SolomonParser::parse(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("Cannot open file: " + path);
    }

    Instance inst;
    inst.num_vehicles = 0;
    inst.capacity     = 0;

    std::vector<std::string> lines;
    {
        std::string line;
        while (std::getline(in, line)) {
            lines.push_back(line);
        }
    }

    if (lines.empty()) {
        throw std::runtime_error("Empty file: " + path);
    }

    // Строка 1: название инстанса.
    inst.name = trim(lines[0]);

    // Ищем первую строку с двумя числами — это num_vehicles и capacity.
    // Находим её среди строк с индексом > 0.
    size_t vehicle_line = std::string::npos;
    for (size_t i = 1; i < lines.size(); ++i) {
        if (!starts_with_digit(lines[i])) continue;
        std::istringstream ss(lines[i]);
        int a, b;
        if ((ss >> a >> b) && !(ss >> a)) { // ровно два числа
            inst.num_vehicles = b; // порядок: NUMBER CAPACITY

            std::istringstream ss2(lines[i]);
            int n1, n2; double x;
            ss2 >> n1 >> n2;
            if (!(ss2 >> x)) {
                inst.num_vehicles = n1;
                inst.capacity     = n2;
                vehicle_line = i;
                break;
            }
        }
    }

    if (vehicle_line == std::string::npos) {
        throw std::runtime_error("Cannot find vehicle/capacity line in: " + path);
    }

    // Читаем строки клиентов — все строки, начинающиеся с цифры, после vehicle_line.
    for (size_t i = vehicle_line + 1; i < lines.size(); ++i) {
        if (!starts_with_digit(lines[i])) continue;

        std::istringstream ss(lines[i]);
        int id, demand, ready, due, service;
        double x, y;
        if (!(ss >> id >> x >> y >> demand >> ready >> due >> service)) continue;

        inst.clients.push_back(Client{id, x, y, demand, ready, due, service});
    }

    if (inst.clients.empty()) {
        throw std::runtime_error("No client data found in: " + path);
    }

    return inst;
}

} // namespace vrp
