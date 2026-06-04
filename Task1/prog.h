#ifndef PROG_H
#define PROG_H

struct Node {
    char data;
    struct Node *next;
};

/* Metrics for sorting algorithms */
struct SortMetrics {
    long comparisons;
    long pointer_swaps;
};

/* Basic list operations */
void create_list(struct Node **p);
int size(const struct Node *p);
void print_list(const struct Node *p);
void push_back(struct Node **p, char value);
void push_front(struct Node **p, char value);
void pop_back(struct Node **p);
void pop_front(struct Node **p);
void insert_node(struct Node **p, int index, char value);
void remove_node(struct Node **p, int index);
void clear(struct Node **p);
void remove_list(struct Node **p);

/* Sorting algorithms */
void insertion_sort(struct Node **p, struct SortMetrics *m);
void merge_sort(struct Node **p, struct SortMetrics *m);

#endif /* PROG_H */