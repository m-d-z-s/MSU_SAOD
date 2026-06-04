#ifndef PROG_HPP
#define PROG_HPP

#include <cstddef>
#include <stdexcept>
#include <iterator>
#include <initializer_list>
#include <utility>

template <typename T>
class LList {
private:
    struct Node {
        T     data;
        Node* next;

        explicit Node(const T& v, Node* n = nullptr) : data(v), next(n) {}
        Node()                                        : data{},  next(nullptr) {}
    };

    Node*  sentinel_; // tail marker: always present, next == nullptr
    Node*  front_;    // first data node, or sentinel_ when empty
    size_t size_;

    // ── Internal helpers ──────────────────────────────────────────────────
    Node* node_at(size_t index) const noexcept;
    Node* prev_of(Node* target) const noexcept;
    void  copy_from(const LList& other);

public:
    // ── Rule of Five ──────────────────────────────────────────────────────
    LList();
    LList(std::initializer_list<T> il);
    ~LList();
    LList(const LList& other);
    LList& operator=(const LList& other);
    LList(LList&& other) noexcept;
    LList& operator=(LList&& other) noexcept;

    // ── Capacity ──────────────────────────────────────────────────────────
    size_t size()  const noexcept;
    bool   empty() const noexcept;

    // ── Element access ────────────────────────────────────────────────────
    T&       operator[](size_t index);
    const T& operator[](size_t index) const;
    const T& front() const;
    const T& back()  const;

    // ── Modifiers ─────────────────────────────────────────────────────────
    void push_back(const T& value);
    void push_front(const T& value);
    void insert(size_t index, const T& value);
    void pop_back();
    void pop_front();
    void remove_at(size_t index);
    void clear() noexcept;

    // ── ListIterator ──────────────────────────────────────────────────────
    class ListIterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type        = T;
        using difference_type   = std::ptrdiff_t;
        using pointer           = T*;
        using reference         = T&;

        explicit ListIterator(Node* node) noexcept : cur_(node) {}

        T&            operator*()  const noexcept { return cur_->data; }
        T*            operator->() const noexcept { return &cur_->data; }
        ListIterator& operator++() noexcept { cur_ = cur_->next; return *this; }
        ListIterator  operator++(int) noexcept { auto tmp = *this; ++(*this); return tmp; }
        bool operator==(const ListIterator& o) const noexcept { return cur_ == o.cur_; }
        bool operator!=(const ListIterator& o) const noexcept { return cur_ != o.cur_; }

    private:
        Node* cur_;
    };

    // ── ConstListIterator ─────────────────────────────────────────────────
    class ConstListIterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type        = T;
        using difference_type   = std::ptrdiff_t;
        using pointer           = const T*;
        using reference         = const T&;

        explicit ConstListIterator(const Node* node) noexcept : cur_(node) {}

        const T&           operator*()  const noexcept { return cur_->data; }
        const T*           operator->() const noexcept { return &cur_->data; }
        ConstListIterator& operator++() noexcept { cur_ = cur_->next; return *this; }
        ConstListIterator  operator++(int) noexcept { auto tmp = *this; ++(*this); return tmp; }
        bool operator==(const ConstListIterator& o) const noexcept { return cur_ == o.cur_; }
        bool operator!=(const ConstListIterator& o) const noexcept { return cur_ != o.cur_; }

    private:
        const Node* cur_;
    };

    ListIterator      begin()  noexcept       { return ListIterator(front_);           }
    ListIterator      end()    noexcept       { return ListIterator(sentinel_);        }
    ConstListIterator begin()  const noexcept { return ConstListIterator(front_);      }
    ConstListIterator end()    const noexcept { return ConstListIterator(sentinel_);   }
    ConstListIterator cbegin() const noexcept { return ConstListIterator(front_);      }
    ConstListIterator cend()   const noexcept { return ConstListIterator(sentinel_);   }
};

#include "prog.cpp"

#endif // PROG_HPP
