#include <iostream>
#include <numeric>
#include "prog.hpp"

template <typename T>
void print_lst(const LList<T>& l) {
    bool first = true;
    for (const auto& v : l) {
        if (!first) std::cout << " -> ";
        std::cout << v;
        first = false;
    }
    std::cout << '\n';
}

static void demo_basic() {
    std::cout << "=== Basic methods ===\n\n";

    LList<char> lst;
    std::cout << std::boolalpha << lst.empty() << '\n';   // true

    for (int i = 0; i < 5; i++)
        lst.push_back(char('a' + i));
    print_lst(lst);                        // a -> b -> c -> d -> e

    for (int i = 0; i < 5; i++)
        lst.insert(0, char('z' - i));
    print_lst(lst);                        // v -> w -> x -> y -> z -> a -> b -> c -> d -> e

    for (size_t i = 0; i != lst.size(); i++)
        lst[i] = char('a' + i);
    print_lst(lst);                        // a -> b -> c -> d -> e -> f -> g -> h -> i -> j

    lst.pop_back();
    lst.pop_front();
    print_lst(lst);                        // b -> c -> d -> e -> f -> g -> h -> i

    lst.remove_at(5);
    lst.insert(3, 'o');
    print_lst(lst);                        // b -> c -> d -> o -> e -> f -> h -> i

    lst.clear();
    lst.push_back('q');
    lst.push_back('w');
    std::cout << lst.size() << ' ' << lst.empty() << '\n'; // 2 false
}

static void demo_rule_of_five() {
    std::cout << "\n=== Rule of Five ===\n\n";

    // Copy constructor
    {
        LList<char> a;
        for (char c = 'a'; c <= 'e'; c++) a.push_back(c);
        LList<char> b = a;
        b[0] = 'Z';
        std::cout << "copy ctor  — a: "; print_lst(a);  // a -> b -> c -> d -> e
        std::cout << "copy ctor  — b: "; print_lst(b);  // Z -> b -> c -> d -> e
    }

    // Copy assignment
    {
        LList<char> a;
        LList<char> b;
        for (char c = 'a'; c <= 'e'; c++) a.push_back(c);
        b = a;
        b[1] = 'X';
        std::cout << "copy assign — a: "; print_lst(a); // a -> b -> c -> d -> e
        std::cout << "copy assign — b: "; print_lst(b); // a -> X -> c -> d -> e
    }

    // Move constructor
    {
        LList<char> a;
        for (char c = 'a'; c <= 'e'; c++) a.push_back(c);
        LList<char> b = std::move(a);
        std::cout << "move ctor  — b: "; print_lst(b);  // a -> b -> c -> d -> e
        std::cout << "size a = " << a.size() << '\n';   // size a = 0
    }

    // Move assignment
    {
        LList<char> a;
        LList<char> b;
        for (char c = 'a'; c <= 'e'; c++) a.push_back(c);
        b = std::move(a);
        std::cout << "move assign — b: "; print_lst(b); // a -> b -> c -> d -> e
    }

    // insert at 0 and at size()
    {
        LList<int> l;
        l.push_back(1);
        l.push_back(2);
        l.push_back(3);
        l.insert(0, 100);
        l.insert(l.size(), 200);
        std::cout << "insert 0/size(): "; print_lst(l); // 100 -> 1 -> 2 -> 3 -> 200
    }
}


int main() {
    demo_basic();
    demo_rule_of_five();
    return 0;
}
