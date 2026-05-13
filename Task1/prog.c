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

/* ── Insertion Sort ──────────────────────────────────────────────────────────
 * Перебираем узлы начиная со второго.  Для каждого узла ищем позицию вставки
 * в уже отсортированной части и переставляем указатели (не копируем данные).
 * Структура списка (sentinel в конце) сохраняется.
 * ─────────────────────────────────────────────────────────────────────────── */
void insertion_sort(struct Node **p, struct SortMetrics *m)
{
    struct Node *sentinel = get_sentinel(*p);
    struct Node *sorted_end; /* последний узел отсортированной части        */
    struct Node *cur;        /* текущий узел для вставки                    */
    struct Node *prev_cur;   /* узел перед cur в исходном списке            */
    struct Node *ins;        /* узел, перед которым вставляем cur           */
    struct Node *prev_ins;   /* узел перед ins                              */

    m->comparisons   = 0;
    m->pointer_swaps = 0;

    /* Список пуст или из одного элемента */
    if (*p == sentinel || (*p)->next == sentinel)
        return;

    sorted_end = *p; /* первый узел уже «отсортирован» сам по себе */

    while (sorted_end->next != sentinel) {
        cur      = sorted_end->next; /* берём первый узел неотсортированной части */
        prev_ins = NULL;
        ins      = *p;

        /* Ищем позицию вставки в отсортированной части */
        while (ins != cur) {
            m->comparisons++;
            if (ins->data > cur->data)
                break;
            prev_ins = ins;
            ins      = ins->next;
        }

        if (ins == cur) {
            /* cur уже на своём месте — просто продвигаем границу */
            sorted_end = cur;
        } else {
            /* Отцепляем cur от текущей позиции */
            prev_cur = *p;
            while (prev_cur->next != cur)
                prev_cur = prev_cur->next;
            prev_cur->next = cur->next;   m->pointer_swaps++;

            /* Вставляем cur перед ins */
            cur->next = ins;              m->pointer_swaps++;
            if (prev_ins == NULL) {
                *p = cur;                 m->pointer_swaps++;
            } else {
                prev_ins->next = cur;     m->pointer_swaps++;
            }
            /* sorted_end не сдвигаем: следующий за ним — всё тот же узел */
        }
    }
}

/* ── Merge Sort (вспомогательные функции) ───────────────────────────────── */

/* Разрезаем список на две половины.
 * slow/fast — классический алгоритм «черепаха и заяц».
 * *front получает первую половину, *back — вторую.
 * sentinel не передаётся: он останется в хвосте второй половины.          */
static void split_list(struct Node *head, struct Node *sentinel,
                       struct Node **front, struct Node **back)
{
    struct Node *slow = head;
    struct Node *fast = head->next;

    while (fast != sentinel && fast->next != sentinel) {
        slow = slow->next;
        fast = fast->next->next;
    }

    *front = head;
    *back  = slow->next;
    slow->next = sentinel; /* обрываем первую половину */
}

/* Слияние двух отсортированных подсписков.
 * Оба заканчиваются одним и тем же sentinel.
 * Возвращает голову объединённого списка.                                   */
static struct Node* merge_lists(struct Node *a, struct Node *b,
                                struct Node *sentinel,
                                struct SortMetrics *m)
{
    struct Node dummy;   /* фиктивный узел-голова результата */
    struct Node *tail = &dummy;
    dummy.next = sentinel;

    while (a != sentinel && b != sentinel) {
        m->comparisons++;
        if (a->data <= b->data) {
            tail->next = a;  m->pointer_swaps++;
            a = a->next;
        } else {
            tail->next = b;  m->pointer_swaps++;
            b = b->next;
        }
        tail = tail->next;
    }

    /* Присоединяем остаток */
    if (a != sentinel) {
        tail->next = a;  m->pointer_swaps++;
    } else {
        tail->next = b;  m->pointer_swaps++;
    }

    /* Убеждаемся, что хвост объединённого списка упирается в sentinel */
    tail = tail->next;
    while (tail->next != sentinel)
        tail = tail->next;
    tail->next = sentinel;

    return dummy.next;
}

/* Рекурсивное ядро merge sort.
 * Принимает и возвращает голову подсписка; sentinel — общий для всех.       */
static struct Node* merge_sort_recursive(struct Node *head,
                                         struct Node *sentinel,
                                         struct SortMetrics *m)
{
    struct Node *front;
    struct Node *back;

    if (head == sentinel || head->next == sentinel)
        return head;

    split_list(head, sentinel, &front, &back);

    front = merge_sort_recursive(front, sentinel, m);
    back  = merge_sort_recursive(back,  sentinel, m);

    return merge_lists(front, back, sentinel, m);
}

/* ── Merge Sort (публичный интерфейс) ───────────────────────────────────── */
void merge_sort(struct Node **p, struct SortMetrics *m)
{
    struct Node *sentinel = get_sentinel(*p);

    m->comparisons   = 0;
    m->pointer_swaps = 0;

    if (*p == sentinel || (*p)->next == sentinel)
        return;

    *p = merge_sort_recursive(*p, sentinel, m);
}
