Additional Example: README.md
markdown

# Examples Directory

This directory contains practical examples demonstrating the usage of the RK3568 DDR Memory Manager.

## 📋 Examples

### 1. basic_config_example.c
Basic DDR configuration and initialization example.

**Features:**
- DDR initialization
- Custom timing configuration
- Memory testing
- Performance testing
- Monitoring demonstration

**Build:**
```bash
gcc -o basic_config_example basic_config_example.c -lddr_manager

Run:
bash

./basic_config_example --board edge2 --test

2. npu_memory_setup.c

NPU memory allocation and inference example.

Features:

    NPU memory pool allocation

    RKNN model loading

    Inference execution

    Memory statistics

    Test inference

Build:
bash

gcc -o npu_memory_setup npu_memory_setup.c -lddr_manager -lrknnrt

Run:
bash

./npu_memory_setup --model model.rknn --test

3. custom_partition_example.c

Custom partition management example.

Features:

    Partition creation

    Partition management

    Partition protection

    Partition tests

    Partition listing

Build:
bash

gcc -o custom_partition_example custom_partition_example.c -lddr_manager

Run:
bash

./custom_partition_example --create --test

🚀 Quick Start
bash

# Clone the repository
git clone https://github.com/yourusername/RK3568-DDR-Memory-Manager.git

# Build all examples
cd RK3568-DDR-Memory-Manager/docs/examples
make all

# Or build specific examples
make basic_config_example
make npu_memory_setup
make custom_partition_example

# Run examples
./basic_config_example --help
./npu_memory_setup --help
./custom_partition_example --help

📖 Documentation

    API Documentation

    Architecture Guide

    Installation Guide

💡 Tips

    Always initialize the subsystem before using any functions

    Check return values for error handling

    Use the monitoring functions for debugging

    Clean up resources properly

    Run with --help for usage information

📝 License

This code is licensed under the MIT License.
text


---

## 🚀 **Makefile for Examples**

```makefile
# Makefile for RK3568 DDR Memory Manager Examples
# Version: 1.0.0

CC = gcc
CFLAGS = -Wall -Wextra -O2 -g
LDFLAGS = -lddr_manager -lrknnrt -lm

TARGETS = basic_config_example npu_memory_setup custom_partition_example

all: $(TARGETS)

basic_config_example: basic_config_example.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

npu_memory_setup: npu_memory_setup.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

custom_partition_example: custom_partition_example.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGETS) *.o

install:
	mkdir -p /usr/local/bin
	cp $(TARGETS) /usr/local/bin/

help:
	@echo "Available targets:"
	@echo "  all      - Build all examples"
	@echo "  clean    - Remove built files"
	@echo "  install  - Install binaries"
	@echo "  help     - Show this help"

.PHONY: all clean install help

💡 Key Insights for Interviews
What This Shows to Interviewers

    Practical Skills: Working code examples

    API Understanding: How to use the interfaces

    Problem Solving: Real-world use cases

    Documentation: Well-commented code

    Testing: Demonstration of functionality

Sample Interview Answer

Interviewer: "Show me how you would use this DDR manager."

You: "Here are three examples. The basic configuration example shows how to initialize DDR with custom timings and test memory performance. The NPU example demonstrates allocating dedicated memory for AI inference. The partition example shows creating protected memory regions for different subsystems. Each example includes error handling and demonstrates best practices."
Examples Overview
Example	Concepts	Difficulty
basic_config_example	Init, config, test	Beginner
npu_memory_setup	NPU, RKNN, inference	Intermediate
custom_partition_example	Partitions, protection	Advanced

These examples demonstrate practical usage and show interviewers you can write production-quality code! 🚀

