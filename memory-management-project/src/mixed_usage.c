#include <stdio.h>
#include <stdlib.h>

static int id = 100; // static memory

int main() {
    int stack_var = 10; // stack memory

    int *heap_var = malloc(sizeof(int)); // heap memory
    *heap_var = 50;

    printf("Static ID = %d\n", id);
    printf("Stack Var = %d\n", stack_var);
    printf("Heap Var = %d\n", *heap_var);

    free(heap_var);
    return 0;
}
