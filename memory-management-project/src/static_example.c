#include <stdio.h>

static int counter = 0;   // static memory

void increase() {
    counter++;
    printf("Counter = %d\n", counter);
}

int main() {
    increase();
    increase();
    increase();
    return 0;
}
