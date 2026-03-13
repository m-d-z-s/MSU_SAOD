#include <stdio.h>
#include <stdlib.h>
#include "prog.h"

static struct Node* get_sentinel(struct Node *p)
{
    while (p->next != NULL)
        p = p->next;
    return p;
}

void create_list(struct Node **p)
{
    *p = (struct Node*)malloc(sizeof(struct Node));
    (*p)->next = NULL;
}

int size(const struct Node *p)
{
    int count = 0;
    while (p->next != NULL) {
        count++;
        p = p->next;
    }
    return count;
}

void print_list(const struct Node *p)
{
    while (p->next != NULL) {
        printf("%c", p->data);
        if (p->next->next != NULL)
            printf(" -> ");
        p = p->next;
    }
    printf("\n\n");
}

void push_back(struct Node **p, char value)
{
    struct Node *sentinel = get_sentinel(*p);
    struct Node *new_node = (struct Node*)malloc(sizeof(struct Node));

    new_node->data = value;
    new_node->next = sentinel;

    if (*p == sentinel) {
        *p = new_node;
    } else {
        struct Node *cur = *p;
        while (cur->next != sentinel)
            cur = cur->next;
        cur->next = new_node;
    }
}

void push_front(struct Node **p, char value)
{
    struct Node *sentinel = get_sentinel(*p);
    struct Node *new_node = (struct Node*)malloc(sizeof(struct Node));

    new_node->data = value;
    new_node->next = *p;

    *p = new_node;

    if (sentinel == *p)
        sentinel->next = NULL;
}

void pop_back(struct Node **p)
{
    struct Node *sentinel = get_sentinel(*p);
    struct Node *cur = *p;
    struct Node *prev = NULL;

    if (*p == sentinel)
        return;

    while (cur->next != sentinel) {
        prev = cur;
        cur = cur->next;
    }

    if (prev == NULL)
        *p = sentinel;
    else
        prev->next = sentinel;

    free(cur);
}

void pop_front(struct Node **p)
{
    struct Node *sentinel = get_sentinel(*p);
    struct Node *tmp;

    if (*p == sentinel)
        return;

    tmp = *p;
    *p = (*p)->next;
    free(tmp);
}

void insert_node(struct Node **p, int index, char value)
{
    struct Node *sentinel = get_sentinel(*p);
    struct Node *cur = *p;
    struct Node *new_node;
    int i;

    if (index == 0) {
        push_front(p, value);
        return;
    }

    for (i = 0; i < index - 1 && cur->next != sentinel; i++)
        cur = cur->next;

    new_node = (struct Node*)malloc(sizeof(struct Node));
    new_node->data = value;
    new_node->next = cur->next;
    cur->next = new_node;
}

void remove_node(struct Node **p, int index)
{
    struct Node *sentinel = get_sentinel(*p);
    struct Node *cur = *p;
    struct Node *prev = NULL;
    int i;

    if (*p == sentinel)
        return;

    if (index == 0) {
        pop_front(p);
        return;
    }

    for (i = 0; i < index && cur->next != sentinel; i++) {
        prev = cur;
        cur = cur->next;
    }

    if (cur->next == NULL)
        return;

    prev->next = cur->next;
    free(cur);
}

void clear(struct Node **p)
{
    struct Node *sentinel = get_sentinel(*p);
    struct Node *cur = *p;
    struct Node *tmp;

    while (cur != sentinel) {
        tmp = cur;
        cur = cur->next;
        free(tmp);
    }

    *p = sentinel;
}

void remove_list(struct Node **p)
{
    struct Node *sentinel = get_sentinel(*p);
    clear(p);
    free(sentinel);
    *p = NULL;
}
