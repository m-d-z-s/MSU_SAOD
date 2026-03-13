#ifndef PROG_H
#define PROG_H

struct Node {
    char data;
    struct Node *next;
};

void create_list(struct Node **p);
void push_back(struct Node **p, char value);
int size(const struct Node *p);
void print_list(const struct Node *p);
void remove_list(struct Node **p);

void push_front(struct Node **p, char value);
void insert_node(struct Node **p, int index, char value);

void pop_back(struct Node **p);
void pop_front(struct Node **p);
void remove_node(struct Node **p, int index);

void clear(struct Node **p);

#endif