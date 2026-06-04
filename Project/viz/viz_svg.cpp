/**
 * @file viz_svg.cpp
 * @brief Генератор анимированного SVG для визуализации решений VRP.
 *
 * Читает инстанс (.vrp / Solomon .txt) и вывод врп-солвера,
 * строит один анимированный SVG-файл со светлой темой.
 *
 * Анимации:
 *   - депо и клиенты появляются по одному с задержкой (fade-in + scale)
 *   - маршруты "рисуются" последовательно через stroke-dashoffset
 *   - числа длин маршрутов всплывают после прорисовки линии
 *   - при наведении на маршрут — подсветка, остальные тускнеют
 *   - легенда появляется в конце
 *
 * Использование:
 *   ./viz_svg <instance_file> <solution_file> [output.svg]
 *   ./viz_svg <instance_file> [output.svg]          — только точки
 *
 *   # Формат Solomon:
 *   ./viz_svg data/C101.txt results/C101.txt routes.svg --format solomon
 *
 * Пример полного pipeline:
 *   ./build/vrp_solver data/A-n32-k5.vrp --algorithm hybrid > results/sol.txt
 *   ./build/viz_svg data/A-n32-k5.vrp results/sol.txt routes.svg
 *   open routes.svg          # macOS
 *   xdg-open routes.svg      # Linux
 *
 * Зависимости: только C++17 STL (no third-party libs).
 */

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// Константы разметки
// ─────────────────────────────────────────────────────────────────────────────

static constexpr int    SVG_W      = 960;
static constexpr int    SVG_H      = 720;
static constexpr int    PAD        = 72;          // отступ от краёв
static constexpr double DEPOT_R    = 11.0;        // радиус депо
static constexpr double CLIENT_R   = 7.0;         // радиус клиента
static constexpr double ROUTE_W    = 2.4;         // толщина линии маршрута
static constexpr int    LEGEND_X   = SVG_W - PAD - 164;
static constexpr int    LEGEND_Y   = PAD + 12;

// Светлая тема
static const char* COLOR_BG        = "#F8F6F1";   // тёплый почти-белый
static const char* COLOR_GRID      = "#E2DDD7";
static const char* COLOR_AXES      = "#C8C2B8";
static const char* COLOR_TEXT      = "#2D2A24";
static const char* COLOR_MUTED     = "#8A8278";
static const char* COLOR_DEPOT     = "#1A1A2E";
static const char* COLOR_DEPOT_STR = "#FFFFFF";

// Палитра маршрутов (10 цветов, при необходимости повторяется)
static const char* ROUTE_COLORS[] = {
    "#E63946",  // красный
    "#2176AE",  // синий
    "#2A9D8F",  // бирюза
    "#E76F51",  // оранжевый
    "#6A4C93",  // фиолетовый
    "#3BB273",  // зелёный
    "#F4A261",  // персиковый
    "#023E8A",  // тёмно-синий
    "#9B2335",  // бордовый
    "#457B9D",  // стальной синий
};
static constexpr int N_COLORS = 10;

// ─────────────────────────────────────────────────────────────────────────────
// Структуры данных
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Один клиент (или депо) инстанса.
 */
struct Client {
    int    id     = 0;
    double x      = 0.0;
    double y      = 0.0;
    int    demand = 0;
};

/**
 * @brief Один маршрут из решения солвера.
 */
struct Route {
    int              index  = 0;   ///< номер маршрута (1-based)
    std::vector<int> nodes;        ///< индексы клиентов (без депо)
    int              demand = 0;   ///< суммарный спрос
    double           length = 0.0; ///< длина маршрута
};

/**
 * @brief Статистика из вывода солвера.
 */
struct SolverStats {
    std::string instance_name;
    std::string algorithm;
    int         num_routes  = 0;
    double      total_len   = 0.0;
    double      time_ms     = 0.0;
    bool        valid       = true;
};

/**
 * @brief Полное состояние для генерации SVG.
 */
struct VizData {
    std::vector<Client> clients;  ///< clients[0] — депо
    int                 capacity = 0;
    std::vector<Route>  routes;
    SolverStats         stats;
};

// ─────────────────────────────────────────────────────────────────────────────
// Вспомогательные строковые функции
// ─────────────────────────────────────────────────────────────────────────────

/** @brief Удаляет пробелы с обеих сторон строки. */
static std::string trim(const std::string& s) {
    auto a = s.find_first_not_of(" \t\r\n");
    auto b = s.find_last_not_of(" \t\r\n");
    return (a == std::string::npos) ? "" : s.substr(a, b - a + 1);
}

/** @brief Переводит строку в верхний регистр. */
static std::string to_upper(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

/** @brief Форматирует double с нужным числом знаков. */
static std::string fmt(double v, int prec = 1) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(prec) << v;
    return ss.str();
}

// ─────────────────────────────────────────────────────────────────────────────
// Парсер .vrp (TSPLIB / CVRPLIB)
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Парсит файл TSPLIB (.vrp).
 *
 * Формат секций: NODE_COORD_SECTION, DEMAND_SECTION, DEPOT_SECTION.
 * @param path  Путь к файлу.
 * @return Список клиентов (clients[0] = депо) и вместимость.
 */
static std::pair<std::vector<Client>, int> parse_vrp(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("Cannot open file: " + path);

    std::map<int, Client> nodes;
    std::map<int, int>    demands;
    int capacity = 0;
    std::string section;
    std::string line;

    while (std::getline(f, line)) {
        line = trim(line);
        if (line.empty()) continue;

        if (line == "NODE_COORD_SECTION") { section = "coords";  continue; }
        if (line == "DEMAND_SECTION")     { section = "demands"; continue; }
        if (line == "DEPOT_SECTION" || line == "EOF") { section = ""; continue; }

        if (line.find(':') != std::string::npos) {
            auto pos = line.find(':');
            auto key = to_upper(trim(line.substr(0, pos)));
            auto val = trim(line.substr(pos + 1));
            if (key == "CAPACITY") capacity = std::stoi(val);
            section = "";
            continue;
        }

        std::istringstream ss(line);
        if (section == "coords") {
            int id; double x, y;
            if (ss >> id >> x >> y) {
                Client c; c.id = id; c.x = x; c.y = y;
                nodes[id] = c;
            }
        } else if (section == "demands") {
            int id, d;
            if (ss >> id >> d) demands[id] = d;
        }
    }

    for (auto& [id, d] : demands)
        if (nodes.count(id)) nodes[id].demand = d;

    // Перенумеровываем: 0 = депо (первый узел в TSPLIB), 1..n = клиенты
    std::vector<Client> result;
    result.reserve(nodes.size());
    for (auto& [id, c] : nodes) result.push_back(c);

    // Оставляем нумерацию TSPLIB — депо id=1, клиенты 2..n
    return {result, capacity};
}

// ─────────────────────────────────────────────────────────────────────────────
// Парсер Solomon (.txt)
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Парсит файл формата Solomon (.txt).
 * @return Список клиентов (clients[0] = депо) и вместимость.
 */
static std::pair<std::vector<Client>, int> parse_solomon(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("Cannot open file: " + path);

    std::vector<Client> clients;
    int capacity = 0;
    bool vehicle_line_next = false;
    std::string line;

    while (std::getline(f, line)) {
        line = trim(line);
        if (line.empty()) continue;

        auto up = to_upper(line);
        if (up.find("VEHICLE") != std::string::npos) {
            vehicle_line_next = true;
            continue;
        }
        if (vehicle_line_next && std::isdigit(static_cast<unsigned char>(line[0]))) {
            std::istringstream ss(line);
            int nv; ss >> nv >> capacity;
            vehicle_line_next = false;
            continue;
        }
        if (up.find("CUSTOMER") != std::string::npos) continue;
        if (up.find("CUST") != std::string::npos)     continue;

        std::istringstream ss(line);
        int id; double x, y; int demand;
        if (ss >> id >> x >> y >> demand) {
            Client c; c.id = id; c.x = x; c.y = y; c.demand = demand;
            clients.push_back(c);
        }
    }
    return {clients, capacity};
}

// ─────────────────────────────────────────────────────────────────────────────
// Парсер вывода солвера
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Разбирает stdout солвера.
 *
 * Строки вида:
 *   Route N [demand=D/C, len=L]: 0 -> 3 -> 7 -> 0
 *   Total length : 312.57
 *   Routes used  : 4
 *
 * @param path  Путь к файлу.
 */
static std::pair<std::vector<Route>, SolverStats> parse_solution(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("Cannot open solution file: " + path);

    std::vector<Route> routes;
    SolverStats        stats;

    // Регулярки
    std::regex re_header(R"(===\s+(.+?)\s*\|\s*(.+?)\s*===)");
    std::regex re_route(
        R"(Route\s+(\d+)\s+\[demand=(\d+)/\d+,\s*len=([0-9.]+)\]:\s+(.+))");
    std::regex re_kv(R"(^([A-Za-z ]+)\s*:\s*(.+)$)");

    std::string line;
    while (std::getline(f, line)) {
        line = trim(line);
        if (line.empty()) continue;

        std::smatch m;
        if (std::regex_search(line, m, re_header)) {
            stats.instance_name = trim(m[1].str());
            stats.algorithm     = trim(m[2].str());
            continue;
        }
        if (std::regex_search(line, m, re_route)) {
            Route r;
            r.index  = std::stoi(m[1].str());
            r.demand = std::stoi(m[2].str());
            r.length = std::stod(m[3].str());
            std::istringstream ss(m[4].str());
            std::string tok;
            while (ss >> tok) {
                if (tok == "->") continue;
                int id = std::stoi(tok);
                if (id != 0) r.nodes.push_back(id);
            }
            routes.push_back(r);
            continue;
        }
        if (std::regex_match(line, m, re_kv)) {
            auto key = to_upper(trim(m[1].str()));
            auto val = trim(m[2].str());
            if (key.find("TOTAL") != std::string::npos &&
                key.find("LEN")   != std::string::npos)
                stats.total_len = std::stod(val);
            else if (key.find("ROUTES") != std::string::npos &&
                     key.find("USED")   != std::string::npos)
                stats.num_routes = std::stoi(val);
            else if (key.find("TIME") != std::string::npos)
                stats.time_ms = std::stod(val);
            else if (key == "VALID")
                stats.valid = (val.find("YES") != std::string::npos);
        }
    }
    return {routes, stats};
}

// ─────────────────────────────────────────────────────────────────────────────
// Трансформация координат
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Маппинг координат инстанса → пиксели SVG.
 *
 * Сохраняет пропорции; по умолчанию Y инвертируется
 * (SVG растёт вниз, координаты инстанса — вверх).
 */
struct Transform {
    double scale    = 1.0;
    double off_x    = PAD;
    double off_y    = PAD;
    double min_x    = 0.0;
    double min_y    = 0.0;

    /**
     * @brief Инициализируется по набору клиентов.
     * @param clients  Список всех клиентов.
     */
    explicit Transform(const std::vector<Client>& clients) {
        if (clients.empty()) return;
        double xmin = clients[0].x, xmax = clients[0].x;
        double ymin = clients[0].y, ymax = clients[0].y;
        for (auto& c : clients) {
            xmin = std::min(xmin, c.x); xmax = std::max(xmax, c.x);
            ymin = std::min(ymin, c.y); ymax = std::max(ymax, c.y);
        }
        double span_x = xmax - xmin ? xmax - xmin : 1.0;
        double span_y = ymax - ymin ? ymax - ymin : 1.0;
        double draw_w = SVG_W - 2.0 * PAD;
        double draw_h = SVG_H - 2.0 * PAD;
        scale = std::min(draw_w / span_x, draw_h / span_y);
        off_x = PAD + (draw_w - span_x * scale) / 2.0;
        off_y = PAD + (draw_h - span_y * scale) / 2.0;
        min_x = xmin;
        min_y = ymin;
    }

    /** @brief Преобразует (x,y) инстанса в (px,py) SVG. */
    void to_px(double x, double y, double& px, double& py) const {
        px = off_x + (x - min_x) * scale;
        py = SVG_H - (off_y + (y - min_y) * scale);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Утилиты SVG-атрибутов
// ─────────────────────────────────────────────────────────────────────────────

/** @brief Сериализует double в строку для SVG-атрибута. */
static std::string d(double v) { return fmt(v, 2); }

/**
 * @brief Вычисляет длину ломаной по набору точек.
 * @param pts  Вектор пар (px, py).
 */
static double polyline_length(const std::vector<std::pair<double,double>>& pts) {
    double len = 0.0;
    for (size_t i = 1; i < pts.size(); ++i) {
        double dx = pts[i].first  - pts[i-1].first;
        double dy = pts[i].second - pts[i-1].second;
        len += std::sqrt(dx*dx + dy*dy);
    }
    return len;
}

// ─────────────────────────────────────────────────────────────────────────────
// Генератор SVG
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Главный генератор SVG-документа.
 *
 * Все элементы пишутся последовательно в std::ostream.
 * Анимации реализованы через SMIL (<animate>, <animateTransform>).
 */
class SvgGenerator {
public:
    /**
     * @brief Генерирует полный SVG и пишет в out.
     * @param data  Входные данные (клиенты, маршруты, статистика).
     * @param out   Выходной поток.
     */
    void generate(const VizData& data, std::ostream& out) {
        const auto& clients = data.clients;
        const auto& routes  = data.routes;
        const auto& stats   = data.stats;

        if (clients.empty()) {
            out << "<!-- empty instance -->\n";
            return;
        }

        // Индекс клиента по id
        std::map<int, const Client*> cmap;
        for (auto& c : clients) cmap[c.id] = &c;

        // Определяем депо: первый клиент или тот, у кого demand == 0
        const Client* depot = &clients[0];
        for (auto& c : clients)
            if (c.demand == 0) { depot = &c; break; }

        Transform tr(clients);

        // Суммарная длина всех маршрутных ломаных в пикселях
        // (нужна для задержек анимации)
        double total_px_len = 0.0;
        for (auto& r : routes) {
            auto pts = route_pixels(r, depot, cmap, tr);
            total_px_len += polyline_length(pts);
        }

        write_header(out);
        write_defs(out, routes, data.capacity);
        write_background(out);
        write_grid(out);

        if (!routes.empty()) {
            write_routes(out, routes, depot, cmap, tr, data.capacity);
        }
        write_clients(out, clients, depot, routes, cmap, tr);
        write_stats_panel(out, stats, data.capacity);
        write_legend(out, routes, data.capacity);
        write_footer(out);
    }

private:
    // ── вспомогательные методы ───────────────────────────────────────────────

    /** @brief Строит последовательность пиксельных координат маршрута. */
    static std::vector<std::pair<double,double>> route_pixels(
        const Route& r,
        const Client* depot,
        const std::map<int, const Client*>& cmap,
        const Transform& tr)
    {
        std::vector<std::pair<double,double>> pts;
        double px, py;
        tr.to_px(depot->x, depot->y, px, py);
        pts.push_back({px, py});
        for (int id : r.nodes) {
            auto it = cmap.find(id);
            if (it == cmap.end()) continue;
            tr.to_px(it->second->x, it->second->y, px, py);
            pts.push_back({px, py});
        }
        tr.to_px(depot->x, depot->y, px, py);
        pts.push_back({px, py});
        return pts;
    }

    /** @brief Строит строку d="" для SVG <path> из вектора точек. */
    static std::string pts_to_path(const std::vector<std::pair<double,double>>& pts) {
        std::ostringstream ss;
        for (size_t i = 0; i < pts.size(); ++i) {
            ss << (i == 0 ? "M " : " L ") << d(pts[i].first) << " " << d(pts[i].second);
        }
        return ss.str();
    }

    // ── секции SVG ───────────────────────────────────────────────────────────

    /** @brief Пишет XML-заголовок и открывающий тег <svg>. */
    static void write_header(std::ostream& out) {
        out << R"(<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg"
     width=")" << SVG_W << R"(" height=")" << SVG_H << R"("
     viewBox="0 0 )" << SVG_W << " " << SVG_H << R"(">
)";
    }

    /**
     * @brief Пишет блок <defs>: маркеры стрелок, фильтры, стили.
     *
     * Каждому маршруту — свой маркер-стрелка с нужным цветом.
     * CSS-класс .route-group управляет hover-подсветкой.
     */
    static void write_defs(std::ostream& out,
                           const std::vector<Route>& routes,
                           int /*capacity*/) {
        out << "<defs>\n";

        // Маркеры стрелок (по одному на маршрут)
        for (size_t i = 0; i < routes.size(); ++i) {
            const char* col = ROUTE_COLORS[i % N_COLORS];
            out << "  <marker id=\"arrow" << i
                << "\" markerWidth=\"9\" markerHeight=\"9\""
                << " refX=\"7\" refY=\"3\" orient=\"auto\" markerUnits=\"userSpaceOnUse\">\n"
                << "    <path d=\"M0,0 L0,6 L8,3 z\" fill=\"" << col << "\" opacity=\"0.9\"/>\n"
                << "  </marker>\n";
        }

        // Тень для депо и клиентов
        out << R"(  <filter id="shadow" x="-30%" y="-30%" width="160%" height="160%">
    <feDropShadow dx="0" dy="2" stdDeviation="3"
                  flood-color="#00000022"/>
  </filter>
  <filter id="glow" x="-40%" y="-40%" width="180%" height="180%">
    <feGaussianBlur stdDeviation="4" result="blur"/>
    <feMerge><feMergeNode in="blur"/><feMergeNode in="SourceGraphic"/></feMerge>
  </filter>
)";

        // CSS: hover-интерактивность маршрутов
        out << R"(  <style>
    .route-group path { transition: opacity 0.25s, stroke-width 0.25s; }
    .route-group circle { transition: opacity 0.25s; }
    svg:hover .route-group { opacity: 0.18; }
    svg:hover .route-group:hover { opacity: 1.0; }
    svg:hover .route-group:hover path { stroke-width: 3.8px; }
    .client-dot { transition: r 0.2s, opacity 0.2s; cursor: default; }
    .client-dot:hover { opacity: 1 !important; }
    .label-len { font: 600 10px/1 'SF Mono', 'Fira Mono', monospace; }
    .stat-key  { font: 400 11px/1 'SF Mono', 'Fira Mono', monospace; fill: #8A8278; }
    .stat-val  { font: 700 11px/1 'SF Mono', 'Fira Mono', monospace; fill: #2D2A24; }
    .panel-bg  { fill: white; fill-opacity: 0.88;
                 stroke: #E2DDD7; stroke-width: 1; rx: 8; }
  </style>
)";
        out << "</defs>\n";
    }

    /** @brief Рисует фон (тёплый оттенок бумаги) и рамку. */
    static void write_background(std::ostream& out) {
        out << "<rect width=\"" << SVG_W << "\" height=\"" << SVG_H << "\""
            << " fill=\"" << COLOR_BG << "\"/>\n";
        // Слабая текстура: диагональные штрихи (SVG pattern)
        out << R"(<defs>
  <pattern id="texture" x="0" y="0" width="8" height="8" patternUnits="userSpaceOnUse">
    <line x1="0" y1="8" x2="8" y2="0" stroke="#00000006" stroke-width="1"/>
  </pattern>
</defs>
<rect width=")" << SVG_W << "\" height=\"" << SVG_H
            << "\" fill=\"url(#texture)\"/>\n";
        // Рамка рабочей области
        out << "<rect x=\"" << PAD << "\" y=\"" << PAD << "\""
            << " width=\"" << SVG_W - 2*PAD << "\" height=\"" << SVG_H - 2*PAD << "\""
            << " fill=\"white\" fill-opacity=\"0.6\""
            << " stroke=\"" << COLOR_AXES << "\" stroke-width=\"0.8\""
            << " rx=\"4\"/>\n";
    }

    /** @brief Рисует лёгкую сетку внутри рабочей области. */
    static void write_grid(std::ostream& out) {
        out << "<g id=\"grid\" opacity=\"0.5\">\n";
        for (int x = PAD + 80; x < SVG_W - PAD; x += 80) {
            out << "  <line x1=\"" << x << "\" y1=\"" << PAD
                << "\" x2=\"" << x << "\" y2=\"" << SVG_H - PAD
                << "\" stroke=\"" << COLOR_GRID << "\" stroke-width=\"0.6\""
                << " stroke-dasharray=\"3,5\"/>\n";
        }
        for (int y = PAD + 60; y < SVG_H - PAD; y += 60) {
            out << "  <line x1=\"" << PAD << "\" y1=\"" << y
                << "\" x2=\"" << SVG_W - PAD << "\" y2=\"" << y
                << "\" stroke=\"" << COLOR_GRID << "\" stroke-width=\"0.6\""
                << " stroke-dasharray=\"3,5\"/>\n";
        }
        out << "</g>\n";
    }

    /**
     * @brief Рисует маршруты с анимацией "рисования" через stroke-dashoffset.
     *
     * Каждый маршрут — отдельная группа <g class="route-group">.
     * Линия анимируется: stroke-dasharray = длина, dashoffset 0→0.
     * После прорисовки всплывает метка с длиной.
     */
    static void write_routes(std::ostream& out,
                             const std::vector<Route>& routes,
                             const Client* depot,
                             const std::map<int, const Client*>& cmap,
                             const Transform& tr,
                             int /*capacity*/)
    {
        // Время начала анимации для каждого маршрута
        // Первые маршруты начинают позже (после появления точек)
        double t_start = 0.6; // секунды, после появления клиентов

        out << "<g id=\"routes\">\n";

        for (size_t i = 0; i < routes.size(); ++i) {
            const auto& r = routes[i];
            const char* col = ROUTE_COLORS[i % N_COLORS];

            auto pts = route_pixels(r, depot, cmap, tr);
            if (pts.size() < 2) continue;

            double px_len  = polyline_length(pts);
            double draw_dur = std::max(0.4, px_len / 600.0); // скорость: 600px/с
            double t_begin  = t_start + static_cast<double>(i) * 0.35;
            double t_label  = t_begin + draw_dur;

            std::string path_d = pts_to_path(pts);
            std::string group_id = "route_" + std::to_string(i);

            out << "  <g class=\"route-group\" id=\"" << group_id << "\">\n";

            // Линия маршрута
            out << "    <path d=\"" << path_d << "\""
                << " fill=\"none\" stroke=\"" << col << "\""
                << " stroke-width=\"" << ROUTE_W << "\""
                << " stroke-linecap=\"round\" stroke-linejoin=\"round\""
                << " stroke-dasharray=\"" << d(px_len) << "\""
                << " stroke-dashoffset=\"" << d(px_len) << "\""
                << " marker-mid=\"url(#arrow" << i << ")\""
                << " opacity=\"0.88\""
                << ">\n";
            // Анимация рисования
            out << "      <animate attributeName=\"stroke-dashoffset\""
                << " from=\"" << d(px_len) << "\" to=\"0\""
                << " dur=\"" << fmt(draw_dur, 2) << "s\""
                << " begin=\"" << fmt(t_begin, 2) << "s\""
                << " fill=\"freeze\" calcMode=\"spline\""
                << " keyTimes=\"0;1\" keySplines=\"0.4 0 0.2 1\"/>\n";
            out << "    </path>\n";

            // Метка длины — появляется в середине маршрута после прорисовки
            if (pts.size() >= 2) {
                size_t mid = pts.size() / 2;
                double lx = (pts[mid].first + pts[mid-1].first) / 2.0 + 4.0;
                double ly = (pts[mid].second + pts[mid-1].second) / 2.0 - 5.0;

                out << "    <g opacity=\"0\">\n";
                // Фоновый прямоугольник метки
                out << "      <rect x=\"" << d(lx - 2) << "\" y=\"" << d(ly - 11)
                    << "\" width=\"42\" height=\"14\""
                    << " rx=\"3\" fill=\"white\" fill-opacity=\"0.85\""
                    << " stroke=\"" << col << "\" stroke-width=\"0.8\"/>\n";
                out << "      <text x=\"" << d(lx + 1) << "\" y=\"" << d(ly)
                    << "\" class=\"label-len\" fill=\"" << col << "\">"
                    << fmt(r.length, 1) << "</text>\n";
                // Анимация появления метки
                out << "      <animate attributeName=\"opacity\""
                    << " from=\"0\" to=\"1\""
                    << " dur=\"0.3s\" begin=\"" << fmt(t_label, 2) << "s\""
                    << " fill=\"freeze\"/>\n";
                out << "    </g>\n";
            }

            out << "  </g>\n";
            t_start = 0.6; // reset for next if needed
        }
        out << "</g>\n";
    }

    /**
     * @brief Рисует клиентов и депо с анимацией появления.
     *
     * Точки появляются по одной с нарастающей задержкой (stagger).
     * Депо рисуется последним (поверх всего), с пульсирующей обводкой.
     */
    static void write_clients(std::ostream& out,
                              const std::vector<Client>& clients,
                              const Client* depot,
                              const std::vector<Route>& routes,
                              const std::map<int, const Client*>& /*cmap*/,
                              const Transform& tr)
    {
        // Маппинг client_id → route_index
        std::map<int, int> client_route;
        for (size_t ri = 0; ri < routes.size(); ++ri)
            for (int id : routes[ri].nodes)
                client_route[id] = static_cast<int>(ri);

        out << "<g id=\"clients\">\n";

        // Клиенты (без депо)
        int idx = 0;
        for (auto& c : clients) {
            if (&c == depot) continue;
            double px, py;
            tr.to_px(c.x, c.y, px, py);

            double delay = 0.08 + idx * 0.028;
            int ri = -1;
            auto it = client_route.find(c.id);
            if (it != client_route.end()) ri = it->second;
            const char* col = (ri >= 0) ? ROUTE_COLORS[ri % N_COLORS] : COLOR_MUTED;

            out << "  <g transform=\"translate(" << d(px) << "," << d(py) << ")\">\n";
            out << "    <circle class=\"client-dot\" r=\"0\" cx=\"0\" cy=\"0\""
                << " fill=\"" << col << "\""
                << " stroke=\"white\" stroke-width=\"1.5\""
                << " filter=\"url(#shadow)\">\n";
            // Появление: r 0 → CLIENT_R
            out << "      <animate attributeName=\"r\""
                << " from=\"0\" to=\"" << CLIENT_R << "\""
                << " dur=\"0.22s\" begin=\"" << fmt(delay, 3) << "s\""
                << " fill=\"freeze\" calcMode=\"spline\""
                << " keyTimes=\"0;1\" keySplines=\"0.34 1.56 0.64 1\"/>\n";
            out << "    </circle>\n";
            // Подпись (demand) — только если не слишком много клиентов
            if (clients.size() <= 60) {
                out << "    <text x=\"0\" y=\"4\""
                    << " font-family=\"'SF Mono','Fira Mono',monospace\""
                    << " font-size=\"7\" font-weight=\"600\""
                    << " text-anchor=\"middle\" fill=\"white\" opacity=\"0\">\n"
                    << c.demand
                    << "\n      <animate attributeName=\"opacity\""
                    << " from=\"0\" to=\"1\" dur=\"0.15s\""
                    << " begin=\"" << fmt(delay + 0.1, 3) << "s\""
                    << " fill=\"freeze\"/>\n"
                    << "    </text>\n";
            }
            out << "  </g>\n";
            ++idx;
        }

        // Депо — рисуется поверх, с пульсацией
        {
            double px, py;
            tr.to_px(depot->x, depot->y, px, py);
            out << "  <g id=\"depot\" transform=\"translate(" << d(px) << "," << d(py) << ")\">\n";
            // Пульсирующая окружность (repeat: indefinite)
            out << "    <circle r=\"" << DEPOT_R + 8 << "\""
                << " fill=\"none\" stroke=\"" << COLOR_DEPOT << "\" stroke-width=\"1.2\""
                << " opacity=\"0.25\">\n"
                << "      <animate attributeName=\"r\""
                << " values=\"" << DEPOT_R + 4 << ";" << DEPOT_R + 14 << ";" << DEPOT_R + 4 << "\""
                << " dur=\"2.4s\" repeatCount=\"indefinite\"/>\n"
                << "      <animate attributeName=\"opacity\""
                << " values=\"0.35;0;0.35\" dur=\"2.4s\" repeatCount=\"indefinite\"/>\n"
                << "    </circle>\n";
            // Основной круг депо
            out << "    <circle r=\"0\" cx=\"0\" cy=\"0\""
                << " fill=\"" << COLOR_DEPOT << "\""
                << " stroke=\"white\" stroke-width=\"2\""
                << " filter=\"url(#shadow)\">\n"
                << "      <animate attributeName=\"r\""
                << " from=\"0\" to=\"" << DEPOT_R << "\""
                << " dur=\"0.3s\" begin=\"0.05s\" fill=\"freeze\""
                << " calcMode=\"spline\" keyTimes=\"0;1\" keySplines=\"0.34 1.56 0.64 1\"/>\n"
                << "    </circle>\n";
            // Буква D
            out << "    <text x=\"0\" y=\"4\""
                << " font-family=\"'SF Mono','Fira Mono',monospace\""
                << " font-size=\"10\" font-weight=\"700\""
                << " text-anchor=\"middle\" fill=\"" << COLOR_DEPOT_STR << "\" opacity=\"0\">\n"
                << "D\n"
                << "      <animate attributeName=\"opacity\""
                << " from=\"0\" to=\"1\" dur=\"0.2s\" begin=\"0.25s\" fill=\"freeze\"/>\n"
                << "    </text>\n";
            out << "  </g>\n";
        }

        out << "</g>\n";
    }

    /**
     * @brief Рисует панель со статистикой (левый нижний угол).
     */
    static void write_stats_panel(std::ostream& out,
                                  const SolverStats& stats,
                                  int capacity) {
        // Собираем строки таблицы
        struct Row { std::string key, val; };
        std::vector<Row> rows;
        if (!stats.instance_name.empty())
            rows.push_back({"instance",  stats.instance_name});
        if (!stats.algorithm.empty())
            rows.push_back({"algorithm", to_upper(stats.algorithm)});
        if (stats.num_routes > 0)
            rows.push_back({"routes",    std::to_string(stats.num_routes)});
        if (stats.total_len > 0)
            rows.push_back({"length",    fmt(stats.total_len, 1)});
        if (capacity > 0)
            rows.push_back({"capacity",  std::to_string(capacity)});
        if (stats.time_ms > 0)
            rows.push_back({"time (ms)", fmt(stats.time_ms, 1)});
        rows.push_back({"valid",     stats.valid ? "YES" : "NO"});

        if (rows.empty()) return;

        int bx = PAD + 8;
        int by = SVG_H - PAD - static_cast<int>(rows.size()) * 18 - 20;
        int bw = 185;
        int bh = static_cast<int>(rows.size()) * 18 + 18;

        // Фоновый прямоугольник
        out << "<rect x=\"" << bx - 6 << "\" y=\"" << by - 10
            << "\" width=\"" << bw << "\" height=\"" << bh
            << "\" rx=\"6\" fill=\"white\" fill-opacity=\"0.88\""
            << " stroke=\"" << COLOR_GRID << "\" stroke-width=\"1\""
            << " opacity=\"0\">\n"
            << "  <animate attributeName=\"opacity\""
            << " from=\"0\" to=\"1\" dur=\"0.4s\" begin=\"1.8s\" fill=\"freeze\"/>\n"
            << "</rect>\n";

        out << "<g opacity=\"0\">\n"
            << "  <animate attributeName=\"opacity\""
            << " from=\"0\" to=\"1\" dur=\"0.4s\" begin=\"1.9s\" fill=\"freeze\"/>\n";

        for (size_t i = 0; i < rows.size(); ++i) {
            int ty = by + static_cast<int>(i) * 18 + 4;
            out << "  <text x=\"" << bx << "\" y=\"" << ty
                << "\" class=\"stat-key\">" << rows[i].key << ":</text>\n";
            out << "  <text x=\"" << bx + 90 << "\" y=\"" << ty
                << "\" class=\"stat-val\">" << rows[i].val << "</text>\n";
        }
        out << "</g>\n";
    }

    /**
     * @brief Рисует легенду маршрутов (правый верхний угол).
     *
     * Каждая строка: цветной прямоугольник + "R1: N stops".
     * Легенда появляется после прорисовки всех маршрутов.
     */
    static void write_legend(std::ostream& out,
                             const std::vector<Route>& routes,
                             int capacity)
    {
        if (routes.empty()) return;

        int item_h = 20;
        int bw     = 175;
        int bh     = static_cast<int>(routes.size()) * item_h + 26;
        int bx     = LEGEND_X;
        int by     = LEGEND_Y;

        out << "<rect x=\"" << bx - 8 << "\" y=\"" << by - 8
            << "\" width=\"" << bw << "\" height=\"" << bh
            << "\" rx=\"6\" fill=\"white\" fill-opacity=\"0.88\""
            << " stroke=\"" << COLOR_GRID << "\" stroke-width=\"1\""
            << " opacity=\"0\">\n"
            << "  <animate attributeName=\"opacity\""
            << " from=\"0\" to=\"1\" dur=\"0.4s\" begin=\"2.0s\" fill=\"freeze\"/>\n"
            << "</rect>\n";

        out << "<g opacity=\"0\">\n"
            << "  <animate attributeName=\"opacity\""
            << " from=\"0\" to=\"1\" dur=\"0.4s\" begin=\"2.1s\" fill=\"freeze\"/>\n";
        out << "  <text x=\"" << bx + bw/2 - 8 << "\" y=\"" << by + 6
            << "\" font-family=\"'SF Mono','Fira Mono',monospace\""
            << " font-size=\"9\" font-weight=\"600\" fill=\"" << COLOR_MUTED << "\""
            << " text-anchor=\"middle\">ROUTES</text>\n";

        for (size_t i = 0; i < routes.size(); ++i) {
            const char* col = ROUTE_COLORS[i % N_COLORS];
            const auto& r   = routes[i];
            int ly = by + 20 + static_cast<int>(i) * item_h;

            // Цветной прямоугольник
            out << "  <rect x=\"" << bx << "\" y=\"" << ly - 8
                << "\" width=\"16\" height=\"8\" rx=\"2\" fill=\"" << col << "\"/>\n";
            // Текст
            out << "  <text x=\"" << bx + 22 << "\" y=\"" << ly
                << "\" font-family=\"'SF Mono','Fira Mono',monospace\""
                << " font-size=\"10\" fill=\"" << COLOR_TEXT << "\">"
                << "R" << r.index << ": " << r.nodes.size() << " stops"
                << " d=" << r.demand << "/" << capacity
                << "</text>\n";
        }
        out << "</g>\n";
    }

    /** @brief Закрывающий тег </svg>. */
    static void write_footer(std::ostream& out) {
        out << "</svg>\n";
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Точка входа
// ─────────────────────────────────────────────────────────────────────────────

/** @brief Выводит справку. */
static void usage(const char* prog) {
    std::cout
        << "Usage:\n"
        << "  " << prog << " <instance> <solution> [output.svg] [--format vrp|solomon]\n"
        << "  " << prog << " <instance> [output.svg] [--format vrp|solomon]\n\n"
        << "Arguments:\n"
        << "  instance    .vrp (TSPLIB) or Solomon .txt\n"
        << "  solution    stdout from vrp_solver (optional)\n"
        << "  output.svg  output file (default: <instance>_viz.svg)\n"
        << "  --format    vrp | solomon  (auto-detected from extension)\n\n"
        << "Example:\n"
        << "  ./build/vrp_solver data/A-n32-k5.vrp --algorithm hybrid > results/sol.txt\n"
        << "  ./build/viz_svg data/A-n32-k5.vrp results/sol.txt routes.svg\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) { usage(argv[0]); return 1; }

    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], "--help") == 0 ||
            std::strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            return 0;
        }

    // ── разбор аргументов ───────────────────────────────────────────────────
    std::string instance_path;
    std::string solution_path;
    std::string output_path;
    std::string format;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--format" && i + 1 < argc) {
            format = argv[++i];
        } else if (instance_path.empty()) {
            instance_path = arg;
        } else if (solution_path.empty() &&
                   arg.find(".svg") == std::string::npos) {
            solution_path = arg;
        } else if (output_path.empty()) {
            output_path = arg;
        }
    }

    if (instance_path.empty()) { usage(argv[0]); return 1; }

    // Автоопределение формата
    if (format.empty()) {
        auto ext = instance_path.rfind('.');
        if (ext != std::string::npos && instance_path.substr(ext) == ".txt")
            format = "solomon";
        else
            format = "vrp";
    }

    // Имя выходного файла по умолчанию
    if (output_path.empty()) {
        auto base = instance_path.rfind('/');
        std::string stem = (base == std::string::npos)
                           ? instance_path
                           : instance_path.substr(base + 1);
        auto dot = stem.rfind('.');
        if (dot != std::string::npos) stem = stem.substr(0, dot);
        output_path = stem + "_viz.svg";
    }

    // ── загрузка данных ─────────────────────────────────────────────────────
    VizData data;
    try {
        if (format == "solomon")
            std::tie(data.clients, data.capacity) = parse_solomon(instance_path);
        else
            std::tie(data.clients, data.capacity) = parse_vrp(instance_path);
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Instance: " << e.what() << "\n";
        return 2;
    }

    if (!solution_path.empty()) {
        try {
            std::tie(data.routes, data.stats) = parse_solution(solution_path);
        } catch (const std::exception& e) {
            std::cerr << "[WARNING] Solution: " << e.what() << "\n";
        }
    }

    // ── генерация SVG ────────────────────────────────────────────────────────
    std::ofstream out(output_path);
    if (!out) {
        std::cerr << "[ERROR] Cannot write: " << output_path << "\n";
        return 3;
    }

    SvgGenerator gen;
    gen.generate(data, out);
    out.close();

    std::cout << "[OK] " << output_path << "\n"
              << "     clients: " << (data.clients.empty() ? 0
                                      : static_cast<int>(data.clients.size()) - 1)
              << ", routes: " << data.routes.size()
              << ", capacity: " << data.capacity << "\n";
    return 0;
}
