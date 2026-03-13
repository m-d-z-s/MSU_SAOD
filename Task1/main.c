#include <stdio.h>
#include "prog.h"

int main()
{
    struct Node* p;
    int i;

    create_list(&p);

    for(i = 0; i < 4; i++)
        push_back(&p, (char)('c' + i));

    printf("size: %d\n", size(p));
    print_list(p);

    push_front(&p, 'b');
    push_front(&p, 'a');

    printf("size: %d\n", size(p));
    print_list(p);

    pop_back(&p);

    printf("size: %d\n", size(p));
    print_list(p);

    pop_front(&p);

    printf("size: %d\n", size(p));
    print_list(p);

    insert_node(&p, 2, 'x');

    printf("size: %d\n", size(p));
    print_list(p);

    remove_node(&p, 2);

    printf("size: %d\n", size(p));
    print_list(p);

    remove_list(&p);

    return 0;
}