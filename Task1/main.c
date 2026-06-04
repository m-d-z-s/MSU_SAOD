#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "prog.h"

/* ── Генерация тестовых данных ─────────────────────────────────────────── */

static void fill_random(struct Node **p, int n)
{
    int i;
    for (i = 0; i < n; i++)
        push_back(p, (char)('a' + rand() % 26));
}

static void fill_sorted(struct Node **p, int n)
{
    int i;
    for (i = 0; i < n; i++)
        push_back(p, (char)('a' + i % 26));
}

static void fill_reverse(struct Node **p, int n)
{
    int i;
    for (i = n - 1; i >= 0; i--)
        push_back(p, (char)('a' + i % 26));
}

static void fill_nearly_sorted(struct Node **p, int n)
{
    struct Node *cur;
    int swaps, i;
    char tmp;

    fill_sorted(p, n);

    swaps = n / 14 + 1;
    for (i = 0; i < swaps; i++) {
        int pos = rand() % (n - 1);
        cur = *p;
        while (pos-- > 0)
            cur = cur->next;
        tmp             = cur->data;
        cur->data       = cur->next->data;
        cur->next->data = tmp;
    }
}

/* ── Вспомогательные функции ───────────────────────────────────────────── */

static void copy_list(struct Node **dst, const struct Node *src)
{
    while (src->next != NULL) {
        push_back(dst, src->data);
        src = src->next;
    }
}

static int is_sorted(const struct Node *p)
{
    while (p->next != NULL && p->next->next != NULL) {
        if (p->data > p->next->data)
            return 0;
        p = p->next;
    }
    return 1;
}

/* ── Замер одного запуска ──────────────────────────────────────────────── */
typedef void (*sort_fn)(struct Node **, struct SortMetrics *);

static double measure_one(const struct Node *src, sort_fn fn,
                          struct SortMetrics *m)
{
    struct Node *p;
    clock_t t0, t1;

    create_list(&p);
    copy_list(&p, src);

    t0 = clock();
    fn(&p, m);
    t1 = clock();

    if (!is_sorted(p))
        fprintf(stderr, "ERROR: list not sorted!\n");

    remove_list(&p);
    return (double)(t1 - t0) / CLOCKS_PER_SEC;
}

/* ── Серия из RUNS запусков ────────────────────────────────────────────── */
#define RUNS 10

static void run_series(const char *label, const struct Node *src,
                       sort_fn fn, FILE *csv)
{
    int r;
    double total_time = 0.0;
    long   total_cmp  = 0;
    long   total_swap = 0;
    struct SortMetrics m;
    int n = size(src);

    for (r = 0; r < RUNS; r++) {
        total_time += measure_one(src, fn, &m);
        total_cmp  += m.comparisons;
        total_swap += m.pointer_swaps;
    }

    fprintf(csv, "%s,%d,%.6f,%ld,%ld\n",
            label, n,
            total_time / RUNS,
            total_cmp  / RUNS,
            total_swap / RUNS);

    printf("  %-45s n=%5d  cmp=%8ld  swaps=%8ld  t=%.6fs\n",
           label, n,
           total_cmp  / RUNS,
           total_swap / RUNS,
           total_time / RUNS);
}

typedef void (*fill_fn)(struct Node **, int);

static void experiment(FILE *csv, sort_fn sfn, const char *sort_name,
                       fill_fn ffn, const char *data_name,
                       int *sizes, int nsizes)
{
    int i;
    struct Node *src;
    char label[128];

    for (i = 0; i < nsizes; i++) {
        create_list(&src);
        ffn(&src, sizes[i]);
        sprintf(label, "%s|%s", sort_name, data_name);
        run_series(label, src, sfn, csv);
        remove_list(&src);
    }
}

/* ── Демо корректности ─────────────────────────────────────────────────── */
static void demo(void)
{
    struct Node *p;
    struct SortMetrics m;
    int i;

    printf("=== Demo: insertion sort ===\n");
    create_list(&p);
    for (i = 0; i < 8; i++)
        push_back(&p, (char)('a' + rand() % 8));
    printf("Before: "); print_list(p);
    insertion_sort(&p, &m);
    printf("After:  "); print_list(p);
    printf("cmp=%ld  swaps=%ld\n\n", m.comparisons, m.pointer_swaps);
    remove_list(&p);

    printf("=== Demo: merge sort ===\n");
    create_list(&p);
    for (i = 0; i < 8; i++)
        push_back(&p, (char)('a' + rand() % 8));
    printf("Before: "); print_list(p);
    merge_sort(&p, &m);
    printf("After:  "); print_list(p);
    printf("cmp=%ld  swaps=%ld\n\n", m.comparisons, m.pointer_swaps);
    remove_list(&p);
}

int main(void)
{
    int sizes[] = { 100, 200, 400, 800, 1600, 3200, 6400 };
    int nsizes  = (int)(sizeof(sizes) / sizeof(sizes[0]));
    FILE *csv;

    srand((unsigned)42);

    demo();

    csv = fopen("results.csv", "w");
    if (!csv) { fprintf(stderr, "Cannot open results.csv\n"); return 1; }
    fprintf(csv, "algorithm|data_type,size,time_sec,comparisons,pointer_swaps\n");

    printf("=== Experimental measurements (%d runs each) ===\n\n", RUNS);

    printf("-- Insertion Sort --\n");
    experiment(csv, insertion_sort, "insertion_sort", fill_random,        "random",        sizes, nsizes);
    experiment(csv, insertion_sort, "insertion_sort", fill_sorted,        "sorted",        sizes, nsizes);
    experiment(csv, insertion_sort, "insertion_sort", fill_reverse,       "reverse",       sizes, nsizes);
    experiment(csv, insertion_sort, "insertion_sort", fill_nearly_sorted, "nearly_sorted", sizes, nsizes);

    printf("\n-- Merge Sort --\n");
    experiment(csv, merge_sort, "merge_sort", fill_random,        "random",        sizes, nsizes);
    experiment(csv, merge_sort, "merge_sort", fill_sorted,        "sorted",        sizes, nsizes);
    experiment(csv, merge_sort, "merge_sort", fill_reverse,       "reverse",       sizes, nsizes);
    experiment(csv, merge_sort, "merge_sort", fill_nearly_sorted, "nearly_sorted", sizes, nsizes);

    fclose(csv);
    printf("\nResults written to results.csv\n");
    return 0;
}
