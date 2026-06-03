#pragma once

#include <string>
#include <vector>

namespace vrp {

/**
 * @brief Клиент (узел графа): координаты, спрос, временное окно.
 *
 * Индекс 0 зарезервирован под депо.
 * Для CVRP поля ready/due/service не используются.
 */
struct Client {
    int    id;          ///< Номер клиента (0 = депо)
    double x;           ///< Координата X
    double y;           ///< Координата Y
    int    demand;      ///< Спрос (0 для депо)
    int    ready;       ///< Начало временного окна (VRPTW)
    int    due;         ///< Конец временного окна (VRPTW)
    int    service;     ///< Время обслуживания (VRPTW)
};

/**
 * @brief Описание задачи маршрутизации.
 *
 * Хранит список клиентов (clients[0] — депо),
 * ёмкость транспортного средства и число машин.
 */
struct Instance {
    std::string        name;       ///< Название инстанса
    std::vector<Client> clients;   ///< Клиенты; clients[0] — депо
    int                capacity;  ///< Ёмкость одного ТС
    int                num_vehicles; ///< Максимальное число ТС (0 = не ограничено)

    /** @brief Число клиентов без депо. */
    int num_clients() const { return static_cast<int>(clients.size()) - 1; }
};

} // namespace vrp
