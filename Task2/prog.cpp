#ifndef PROG_CPP
#define PROG_CPP

#include "prog.hpp"

// ── Internal helpers ────────────────────────────────────────────────────────

template <typename T>
typename LList<T>::Node* LList<T>::node_at(size_t index) const noexcept {
    Node* cur = front_;
    for (size_t i = 0; i < index; ++i)
        cur = cur->next;
    return cur;
}

template <typename T>
typename LList<T>::Node* LList<T>::prev_of(Node* target) const noexcept {
    Node* cur = front_;
    while (cur->next != target)
        cur = cur->next;
    return cur;
}

template <typename T>
void LList<T>::copy_from(const LList& other) {
    const Node* src = other.front_;
    Node** dst = &front_;
    while (src != other.sentinel_) {
        *dst = new Node(src->data, sentinel_);
        dst  = &((*dst)->next);
        src  = src->next;
    }
    *dst  = sentinel_;
    size_ = other.size_;
}

// ── Rule of Five ────────────────────────────────────────────────────────────

template <typename T>
LList<T>::LList()
    : sentinel_(new Node{}), front_(sentinel_), size_(0) {}

template <typename T>
LList<T>::LList(std::initializer_list<T> il) : LList() {
    for (const auto& v : il)
        push_back(v);
}

template <typename T>
LList<T>::~LList() {
    clear();
    delete sentinel_;
}

template <typename T>
LList<T>::LList(const LList& other)
    : sentinel_(new Node{}), front_(sentinel_), size_(0) {
    copy_from(other);
}

template <typename T>
LList<T>& LList<T>::operator=(const LList& other) {
    if (this != &other) {
        clear();
        copy_from(other);
    }
    return *this;
}

template <typename T>
LList<T>::LList(LList&& other) noexcept
    : sentinel_(other.sentinel_), front_(other.front_), size_(other.size_) {
    other.sentinel_ = new Node{};
    other.front_    = other.sentinel_;
    other.size_     = 0;
}

template <typename T>
LList<T>& LList<T>::operator=(LList&& other) noexcept {
    if (this != &other) {
        clear();
        delete sentinel_;

        sentinel_ = other.sentinel_;
        front_    = other.front_;
        size_     = other.size_;

        other.sentinel_ = new Node{};
        other.front_    = other.sentinel_;
        other.size_     = 0;
    }
    return *this;
}

// ── Capacity ─────────────────────────────────────────────────────────────────

template <typename T>
size_t LList<T>::size() const noexcept { return size_; }

template <typename T>
bool LList<T>::empty() const noexcept { return size_ == 0; }

// ── Element access ───────────────────────────────────────────────────────────

template <typename T>
T& LList<T>::operator[](size_t index) {
    return node_at(index)->data;
}

template <typename T>
const T& LList<T>::operator[](size_t index) const {
    return node_at(index)->data;
}

template <typename T>
const T& LList<T>::front() const {
    if (empty()) throw std::out_of_range("front() called on empty list");
    return front_->data;
}

template <typename T>
const T& LList<T>::back() const {
    if (empty()) throw std::out_of_range("back() called on empty list");
    return prev_of(sentinel_)->data;
}

// ── Modifiers ────────────────────────────────────────────────────────────────

template <typename T>
void LList<T>::push_back(const T& value) {
    Node* new_node = new Node(value, sentinel_);
    if (front_ == sentinel_) {
        front_ = new_node;
    } else {
        prev_of(sentinel_)->next = new_node;
    }
    ++size_;
}

template <typename T>
void LList<T>::push_front(const T& value) {
    front_ = new Node(value, front_);
    ++size_;
}

template <typename T>
void LList<T>::insert(size_t index, const T& value) {
    if (index == 0) {
        push_front(value);
        return;
    }
    if (index >= size_) {
        push_back(value);
        return;
    }
    Node* pred = node_at(index - 1);
    pred->next = new Node(value, pred->next);
    ++size_;
}

template <typename T>
void LList<T>::pop_back() {
    if (empty()) return;
    if (front_->next == sentinel_) {
        // Only one element
        delete front_;
        front_ = sentinel_;
    } else {
        Node* last   = prev_of(sentinel_);
        Node* second = prev_of(last);
        second->next = sentinel_;
        delete last;
    }
    --size_;
}

template <typename T>
void LList<T>::pop_front() {
    if (empty()) return;
    Node* old = front_;
    front_ = front_->next;
    delete old;
    --size_;
}

template <typename T>
void LList<T>::remove_at(size_t index) {
    if (index == 0) {
        pop_front();
        return;
    }
    Node* pred   = node_at(index - 1);
    Node* target = pred->next;
    if (target == sentinel_) return; // index out of range
    pred->next = target->next;
    delete target;
    --size_;
}

template <typename T>
void LList<T>::clear() noexcept {
    Node* cur = front_;
    while (cur != sentinel_) {
        Node* tmp = cur;
        cur = cur->next;
        delete tmp;
    }
    front_ = sentinel_;
    size_  = 0;
}

#endif // PROG_CPP
