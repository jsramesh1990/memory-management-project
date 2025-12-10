Demonstrate stack, static, and heap memory usage in a real project.

Project Structure
memory-management-project/
│
├── docs/
│   ├── README.md
│   ├── static_memory.md
│   ├── dynamic_memory.md
│   ├── memory_diagrams.png
│
├── src/
│   ├── static_example.c
│   ├── dynamic_example.c
│   ├── mixed_usage.c
│
├── tests/
│   ├── run_tests.sh
│
└── LICENSE

✅ 4. Code Examples for Your GitHub
static_example.c
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


✔ Stored in static region
✔ Value remains across function calls

dynamic_example.c
#include <stdio.h>
#include <stdlib.h>

int main() {
    int n = 5;
    int *arr = malloc(n * sizeof(int));  // dynamic memory

    for (int i = 0; i < n; i++) {
        arr[i] = i + 1;
        printf("%d ", arr[i]);
    }

    free(arr);  // important!
    return 0;
}


✔ Stored in heap
✔ Size flexible and runtime controlled

🔥 5. Add README for GitHub (Copy-Paste)

Here is your complete README:

Memory Management in C – Static vs Dynamic

This project demonstrates how memory works in real C projects using stack, static, and heap memory.

Folder Structure

src/ – C programs for memory demonstration

docs/ – Diagrams and explanations

tests/ – Shell script for testing programs

Topics Covered

Static memory (compile-time)

Dynamic memory (runtime)

Stack vs Heap

Real project examples

Correct usage of malloc/free

How to Run
gcc src/static_example.c -o static
./static

gcc src/dynamic_example.c -o dynamic
./dynamic
