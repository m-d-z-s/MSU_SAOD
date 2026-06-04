/**
 * САОД. Задание 3: Префиксное дерево для подсчёта слов.
 * 
 * Запуск:
 *   ./trie [файл] [слово]
 *   ./trie simplewiki-20260201.txt Dubna
 */

#include <iostream>
#include <fstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <numeric>
#include <algorithm>
#include <cstring>
#include <cstdint>
#include <iomanip>
#include <cassert>

// ─────────────────────────────────────────────────────────────────────────────
// Алфавит: a-z (0-25), A-Z (26-51), ' (52), - (53)  →  54 символа
// ─────────────────────────────────────────────────────────────────────────────
static constexpr int ALPHA = 54;

// Таблица: ASCII-код → индекс в алфавите (-1 = не буква слова)
static int8_t CHAR_TO_IDX[256];

static void init_char_table() noexcept {
    std::memset(CHAR_TO_IDX, -1, sizeof(CHAR_TO_IDX));
    for (int i = 0; i < 26; ++i) {
        CHAR_TO_IDX[static_cast<unsigned char>('a' + i)] = static_cast<int8_t>(i);
        CHAR_TO_IDX[static_cast<unsigned char>('A' + i)] = static_cast<int8_t>(26 + i);
    }
    CHAR_TO_IDX[static_cast<unsigned char>('\'')] = 52;
    CHAR_TO_IDX[static_cast<unsigned char>('-')]  = 53;
}

// ─────────────────────────────────────────────────────────────────────────────
// Интерфейс
// ─────────────────────────────────────────────────────────────────────────────
struct ITrie {
    virtual ~ITrie() = default;
    virtual void   insert(std::string_view str) = 0;
    virtual size_t get(std::string_view str) const = 0;
    virtual size_t nodes() const = 0;  // O(1)
    virtual size_t size()  const = 0;  // O(1)
};

// ─────────────────────────────────────────────────────────────────────────────
// Узел дерева: 54 дочерних узла + счётчик вхождений
// ─────────────────────────────────────────────────────────────────────────────
struct Node {
    uint32_t ch[ALPHA]; // индексы дочерних узлов (0 = нет узла)
    uint32_t count;     // количество вхождений слова, заканчивающегося здесь
};
// 54*4 + 4 = 220 байт на узел

// ─────────────────────────────────────────────────────────────────────────────
// Префиксное дерево на пуле узлов
// ─────────────────────────────────────────────────────────────────────────────
class Trie : public ITrie {
    std::vector<Node> pool_;    // пул узлов; pool_[0] — корень
    uint32_t          node_count_;
    uint32_t          word_count_;

public:
    /**
     * @param reserve  ёмкость пула (узлов). Включается в замер времени.
     */
    explicit Trie(size_t reserve = 5'100'000)
        : node_count_(1), word_count_(0)
    {
        pool_.reserve(reserve);
        pool_.push_back(Node{}); // корень — индекс 0, нулевая инициализация
    }

    /**
     * Однопроходное построение словаря непосредственно из сырого текста.
     * Промежуточные строки не создаются — для каждого символа просто
     * спускаемся по дереву.
     */
    void build(const char* __restrict__ data, size_t n) noexcept {
        uint32_t cur     = 0;
        bool     in_word = false;

        for (size_t i = 0; i < n; ++i) {
            const int8_t idx = CHAR_TO_IDX[static_cast<unsigned char>(data[i])];
            if (idx >= 0) {
                in_word = true;
                if (pool_[cur].ch[idx] == 0) {
                    // Резерва достаточно — перевыделения не будет,
                    // поэтому ссылка pool_[cur] остаётся валидной после push_back.
                    pool_[cur].ch[idx] = node_count_++;
                    pool_.push_back(Node{});
                }
                cur = pool_[cur].ch[idx];
            } else if (in_word) {
                if (pool_[cur].count++ == 0) ++word_count_;
                cur     = 0;
                in_word = false;
            }
        }
        // Последнее слово (если текст не заканчивается разделителем)
        if (in_word) {
            if (pool_[cur].count++ == 0) ++word_count_;
        }
    }

    // ── ITrie ──────────────────────────────────────────────────────────────
    void insert(std::string_view s) override {
        uint32_t cur = 0;
        for (unsigned char c : s) {
            const int8_t idx = CHAR_TO_IDX[c];
            if (idx < 0) return; // символ вне алфавита — прерываем
            if (pool_[cur].ch[idx] == 0) {
                pool_[cur].ch[idx] = node_count_++;
                pool_.push_back(Node{});
            }
            cur = pool_[cur].ch[idx];
        }
        if (pool_[cur].count++ == 0) ++word_count_;
    }

    size_t get(std::string_view s) const override {
        uint32_t cur = 0;
        for (unsigned char c : s) {
            const int8_t idx = CHAR_TO_IDX[c];
            if (idx < 0) return 0;
            const uint32_t nxt = pool_[cur].ch[idx];
            if (!nxt) return 0;
            cur = nxt;
        }
        return pool_[cur].count;
    }

    size_t nodes() const override { return node_count_; }
    size_t size()  const override { return word_count_; }

    // Сброс до состояния «только корень» без освобождения памяти
    void reset() noexcept {
        pool_.resize(1);
        pool_[0] = Node{};
        node_count_ = 1;
        word_count_ = 0;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Хеш-таблица (unordered_map<string_view, size_t>)
// Используется string_view → ключи указывают в исходный буфер, без копий.
// ─────────────────────────────────────────────────────────────────────────────
struct HashResult {
    size_t dict_size;
    size_t word_count;
};

static HashResult umap_build(const char* data, size_t n, std::string_view word) {
    std::unordered_map<std::string_view, size_t> dict;
    dict.reserve(1'600'000); // чуть больше ожидаемого числа уникальных слов

    size_t start   = 0;
    bool   in_word = false;

    for (size_t i = 0; i < n; ++i) {
        const int8_t idx = CHAR_TO_IDX[static_cast<unsigned char>(data[i])];
        if (idx >= 0) {
            if (!in_word) { start = i; in_word = true; }
        } else if (in_word) {
            ++dict[{data + start, i - start}];
            in_word = false;
        }
    }
    if (in_word) ++dict[{data + start, n - start}];

    size_t wc = 0;
    if (auto it = dict.find(word); it != dict.end()) wc = it->second;
    return {dict.size(), wc};
}

// ─────────────────────────────────────────────────────────────────────────────
// Проверка корректности
// ─────────────────────────────────────────────────────────────────────────────
bool verify(const Trie& trie,
            const char* data, size_t n,
            std::string_view word)
{
    const auto [dict_size, word_count] = umap_build(data, n, word);

    bool ok = (trie.size() == dict_size) && (trie.get(word) == word_count);
    if (!ok) {
        std::cerr << "VERIFY FAILED:\n"
                  << "  trie.size()   = " << trie.size()      << " (ожидалось " << dict_size  << ")\n"
                  << "  trie.get(\""  << word << "\") = " << trie.get(word)
                  << " (ожидалось " << word_count << ")\n";
    }
    return ok;
}

// ─────────────────────────────────────────────────────────────────────────────
// Замер времени
// ─────────────────────────────────────────────────────────────────────────────
using Clock = std::chrono::high_resolution_clock;

// Время постройки trie (включая резервирование пула — per условию задачи).
static double bench_trie(const char* data, size_t n) {
    const auto t1 = Clock::now();
    Trie trie(5'100'000);   // ← резервирование внутри замера
    trie.build(data, n);
    const auto t2 = Clock::now();
    return std::chrono::duration<double>(t2 - t1).count();
}

// Время постройки словаря на unordered_map (включая reserve).
static double bench_hash(const char* data, size_t n, std::string_view word) {
    const auto t1 = Clock::now();
    umap_build(data, n, word);
    const auto t2 = Clock::now();
    return std::chrono::duration<double>(t2 - t1).count();
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    init_char_table();

    const char*      filename = (argc > 1) ? argv[1] : "engwiki-ascii-20260201_1gb.txt";
    const std::string word_s  = (argc > 2) ? argv[2] : "Dubna";
    const std::string_view word = word_s;
    constexpr int RUNS = 10;

    // ── Загрузка файла ───────────────────────────────────────────────────────
    std::string text;
    {
        std::ifstream fin(filename, std::ios::binary | std::ios::ate);
        if (!fin.is_open()) {
            std::cerr << "Не удалось открыть файл: " << filename << "\n";
            return 1;
        }
        const std::streamsize sz = fin.tellg();
        fin.seekg(0);
        text.resize(static_cast<size_t>(sz));
        fin.read(text.data(), sz);
        std::cout << "Загружено " << sz << " байт из \"" << filename << "\"\n";
    }

    const char*  data = text.data();
    const size_t n    = text.size();

    // ── Однократная проверка корректности ────────────────────────────────────
    {
        Trie trie(5'100'000);
        trie.build(data, n);
        std::cout << "Trie : size=" << trie.size()
                  << " nodes=" << trie.nodes()
                  << " \"" << word << "\"=" << trie.get(word) << "\n";

        const auto [ds, wc] = umap_build(data, n, word);
        std::cout << "Hash : size=" << ds
                  << " \"" << word << "\"=" << wc << "\n";

        if (!verify(trie, data, n, word)) return 1;
        std::cout << "Correctness: OK\n\n";
    }

    // ── Замеры производительности ─────────────────────────────────────────────
    std::vector<double> hash_times(RUNS), trie_times(RUNS);

    std::cout << "Запуск " << RUNS << " итераций hash...\n";
    for (int i = 0; i < RUNS; ++i) {
        hash_times[i] = bench_hash(data, n, word);
        std::cout << "  [" << i+1 << "/" << RUNS << "] " << hash_times[i] << " с\n";
    }

    std::cout << "Запуск " << RUNS << " итераций trie...\n";
    for (int i = 0; i < RUNS; ++i) {
        trie_times[i] = bench_trie(data, n);
        std::cout << "  [" << i+1 << "/" << RUNS << "] " << trie_times[i] << " с\n";
    }

    // ── Вывод результатов ─────────────────────────────────────────────────────
    std::cout << "\n";
    auto print_vec = [](const std::string& name, const std::vector<double>& v) {
        std::cout << name << ": {";
        for (size_t i = 0; i < v.size(); ++i) {
            if (i) std::cout << ", ";
            std::cout << std::fixed << std::setprecision(5) << v[i];
        }
        std::cout << "}\n";
    };
    print_vec("hash", hash_times);
    print_vec("trie", trie_times);

    const double avg_h = std::accumulate(hash_times.begin(), hash_times.end(), 0.0) / RUNS;
    const double avg_t = std::accumulate(trie_times.begin(), trie_times.end(), 0.0) / RUNS;

    std::cout << "Среднее ускорение: "
              << std::fixed << std::setprecision(5)
              << (avg_h / avg_t) << "x\n";


    return 0;
}
