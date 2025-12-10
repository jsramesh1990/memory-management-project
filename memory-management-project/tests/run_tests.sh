#!/bin/bash
gcc ../src/static_example.c -o static
gcc ../src/dynamic_example.c -o dynamic
gcc ../src/mixed_usage.c -o mixed

echo "Running Static Example:"
./static

echo "Running Dynamic Example:"
./dynamic

echo "Running Mixed Usage Example:"
./mixed
