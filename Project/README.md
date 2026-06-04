# VRP Solver

Решатель задач маршрутизации транспорта (Vehicle Routing Problem).

## Реализованные алгоритмы

### Конструктивные эвристики
- **Nearest Neighbor** — жадный, ближайший сосед от депо
- **Clarke-Wright** — параллельная версия (Clarke & Wright, 1964)

### Локальный поиск
- **2-opt** — переворот отрезка внутри маршрута
- **Or-opt** — перенос сегментов из 1–3 клиентов
- **Relocate + Swap** — межмаршрутные операторы

### Метаэвристики
- **Simulated Annealing** — с адаптивным охлаждением
- **Tabu Search** — с критерием аспирации

### Собственная модификация (гибрид)
`CW → 2-opt → Or-opt → Relocate/Swap → SA`  
Взвешенная функция оценки: `total_length + penalty × num_routes`

## Сборка

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Запуск

```bash
./build/vrp_solver data/A/A-n32-k5.vrp --algorithm hybrid
./build/vrp_solver data/Solomon?C101.txt --format solomon --algorithm sa
```

Параметры:
```
--algorithm <algo>   nn | cw | cw2opt | sa | ts | hybrid  (default: hybrid)
--format <fmt>       vrp | solomon                         (default: vrp)
--seed <n>           Начальное значение ГСЧ                (default: 42)
--iters <n>          Макс. итераций SA без улучшения       (default: 50000)
--penalty <p>        Штраф за транспортное средство        (default: 10.0)
--t-initial <t>      Начальная температура SA              (default: 100.0)
--alpha <a>          Коэффициент охлаждения SA             (default: 0.995)
```

## Структура проекта

```
vrp/
├── CMakeLists.txt          # CMake (основная система сборки)
├── Makefile                # Альтернативный Makefile
├── README.md
├── src/
│   ├── core/               # Типы данных, матрица расстояний
│   ├── parsers/            # TSPLIB (.vrp) и Solomon (.txt)
│   ├── heuristics/         # NN, Clarke-Wright
│   ├── local_search/       # 2-opt, Or-opt, Relocate/Swap
│   ├── metaheuristics/     # SA, Tabu Search
│   ├── hybrid/             # Гибридный решатель (модификация)
│   └── main.cpp
├── tests/                  # Юнит-тесты
└── benchmarks/             # Прогон и CSV-вывод
```

## Данные

Поддерживаются форматы:
- **TSPLIB** (`.vrp`) — бенчмарки CVRPLIB
- **Solomon** (`.txt`) — бенчмарки VRPTW

## Требования

- C++17
- GCC 9+ или Clang 10+
- CMake 3.16+
