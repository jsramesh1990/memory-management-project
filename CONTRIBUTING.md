# CONTRIBUTING.md

```markdown
# Contributing to RK3568 DDR Memory Manager

First off, thank you for considering contributing to this project! It's people like you that make this project better for everyone. This document provides guidelines and instructions for contributing.

## 📋 Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [How Can I Contribute?](#how-can-i-contribute)
- [Development Workflow](#development-workflow)
- [Coding Standards](#coding-standards)
- [Commit Message Guidelines](#commit-message-guidelines)
- [Testing Guidelines](#testing-guidelines)
- [Documentation Standards](#documentation-standards)
- [Pull Request Process](#pull-request-process)
- [Issue Reporting](#issue-reporting)
- [Community](#community)

## 🤝 Code of Conduct

This project and everyone participating in it is governed by our [Code of Conduct](CODE_OF_CONDUCT.md). By participating, you are expected to uphold this code. Please report unacceptable behavior to [project-email@example.com](mailto:project-email@example.com).

### Our Pledge

We pledge to make participation in our project and our community a harassment-free experience for everyone, regardless of age, body size, disability, ethnicity, sex characteristics, gender identity and expression, level of experience, education, socio-economic status, nationality, personal appearance, race, religion, or sexual identity and orientation.

## 🚀 Getting Started

### Prerequisites

Before you begin contributing, ensure you have:

1. **Basic Requirements**:
   - Understanding of C programming
   - Familiarity with embedded systems
   - Knowledge of ARM architecture (specifically ARM64)
   - Understanding of memory management concepts

2. **Development Environment**:
   ```bash
   # Install required packages
   sudo apt-get update
   sudo apt-get install -y \
       build-essential \
       gcc-arm-linux-gnueabihf \
       binutils-arm-linux-gnueabihf \
       git \
       make \
       python3 \
       python3-pip \
       device-tree-compiler \
       u-boot-tools
   ```

3. **Clone the Repository**:
   ```bash
   git clone https://github.com/yourusername/RK3568-DDR-Memory-Manager.git
   cd RK3568-DDR-Memory-Manager
   ```

### Quick Start for Contributors

```bash
# Build the project
./scripts/build/build.sh

# Run tests
./scripts/test/run_tests.sh

# Run the DDR memory manager
./src/user-space/tools/ddr_info_tool
```

## 💡 How Can I Contribute?

### 1. 🐛 Bug Reports

**Before submitting a bug report:**
- Check if the bug has already been reported in [Issues](https://github.com/yourusername/RK3568-DDR-Memory-Manager/issues)
- Ensure you're using the latest version
- Try to reproduce the issue with a clean build

**Good Bug Report Template:**
```markdown
## Bug Description
[Clear description of the bug]

## Steps to Reproduce
1. [First step]
2. [Second step]
3. [...]

## Expected Behavior
[What should happen]

## Actual Behavior
[What actually happens]

## Environment
- Board: [e.g., Mixtile Edge 2]
- OS: [e.g., Ubuntu 20.04]
- Kernel version: [e.g., 5.10.110]
- DDR Memory Manager version: [e.g., 1.0.0]

## Additional Context
- Logs: [Paste relevant logs]
- Configuration: [Attach config files]
- Screenshots: [If applicable]
```

### 2. 🔧 Feature Requests

**Before submitting a feature request:**
- Check if the feature is already requested or implemented
- Consider if the feature fits the project's scope

**Good Feature Request Template:**
```markdown
## Problem Statement
[What problem does this feature solve?]

## Proposed Solution
[How do you propose to implement this?]

## Alternative Solutions
[What other solutions have you considered?]

## Benefits
[Why is this feature important?]

## Implementation Details
[Technical details about implementation]

## Additional Context
[Any other relevant information]
```

### 3. 📚 Documentation Improvements

We welcome documentation contributions! Areas that need improvement:
- API documentation
- Tutorials and guides
- Code comments
- README improvements
- Architecture documentation

### 4. 💻 Code Contributions

#### Areas We Need Help With:
1. **Core Features**:
   - DDR configuration optimization
   - NPU memory management
   - Performance improvements
   - New board support

2. **Tools**:
   - Debugging tools
   - Performance monitoring
   - Configuration generators

3. **Examples**:
   - Smart home integrations
   - AI model examples
   - Custom hardware support

4. **Testing**:
   - Unit tests
   - Integration tests
   - Performance benchmarks

## 🛠 Development Workflow

### 1. Fork and Clone

```bash
# Fork the repository on GitHub
# Then clone your fork
git clone https://github.com/your-username/RK3568-DDR-Memory-Manager.git
cd RK3568-DDR-Memory-Manager

# Add upstream remote
git remote add upstream https://github.com/yourusername/RK3568-DDR-Memory-Manager.git
```

### 2. Create a Branch

```bash
# Create a branch for your feature/fix
git checkout -b feature/your-feature-name
# or
git checkout -b fix/your-bug-fix
```

### 3. Development

```bash
# Make your changes
# Build the project
./scripts/build/build.sh

# Run tests
./scripts/test/run_tests.sh

# Run linting
./scripts/build/lint.sh

# Check code formatting
./scripts/build/format.sh --check
```

### 4. Commit Your Changes

```bash
# Stage your changes
git add .

# Commit with a descriptive message
git commit -m "feat: add NPU memory allocation support

- Implemented memory pool for NPU
- Added device tree configuration
- Updated documentation"

# Push to your fork
git push origin feature/your-feature-name
```

### 5. Create a Pull Request

1. Go to the [Pull Requests](https://github.com/yourusername/RK3568-DDR-Memory-Manager/pulls) page
2. Click "New Pull Request"
3. Select your branch
4. Fill out the PR template
5. Submit the PR

## 📝 Coding Standards

### C Coding Standards

```c
/**
 * File: ddr_layout.c
 * Description: DDR layout management for RK3568
 * Author: Your Name <your.email@example.com>
 * Created: 2024-01-01
 */

#include "ddr_layout.h"
#include <linux/kernel.h>

/**
 * ddr_layout_init - Initialize DDR memory layout
 * @board_type: Type of board (RK3568_EDGE2, RK3568_ROCK3B, etc.)
 * 
 * This function sets up the memory layout based on board type
 * 
 * Return: 0 on success, negative error code on failure
 */
int ddr_layout_init(enum board_type board)
{
    int ret = 0;
    struct ddr_layout *layout = NULL;
    
    /* Validate input */
    if (board >= BOARD_MAX) {
        pr_err("Invalid board type: %d\n", board);
        return -EINVAL;
    }
    
    /* Allocate layout structure */
    layout = kzalloc(sizeof(*layout), GFP_KERNEL);
    if (!layout) {
        pr_err("Failed to allocate layout\n");
        return -ENOMEM;
    }
    
    /* Configure based on board */
    switch (board) {
    case BOARD_MIXTILE_EDGE2:
        ret = setup_mixtile_edge2_layout(layout);
        break;
    case BOARD_RADXA_ROCK3B:
        ret = setup_radxa_rock3b_layout(layout);
        break;
    default:
        ret = setup_default_layout(layout);
        break;
    }
    
    if (ret) {
        kfree(layout);
        return ret;
    }
    
    /* Register layout */
    ret = register_ddr_layout(layout);
    if (ret) {
        kfree(layout);
        return ret;
    }
    
    return 0;
}
EXPORT_SYMBOL_GPL(ddr_layout_init);

/* Naming Conventions */
- Functions: ddr_layout_init()
- Variables: struct ddr_layout *layout
- Macros: #define DDR_MAX_SIZE (512 * 1024 * 1024)
- Enums: enum board_type
- Constants: static const int MAX_RETRIES = 3
```

### Python Coding Standards

```python
#!/usr/bin/env python3
"""
DDR Memory Analyzer Tool

This script analyzes memory usage and generates reports.
"""

import os
import sys
from typing import Dict, List, Optional
from dataclasses import dataclass

@dataclass
class MemoryRegion:
    """Represents a memory region in the DDR layout"""
    name: str
    start: int
    end: int
    size: int
    type: str

class DDRMemoryAnalyzer:
    """
    Analyzer for DDR memory usage patterns.
    
    Attributes:
        memory_regions: List of all memory regions
        usage_data: Current memory usage data
    """
    
    def __init__(self, config_path: str):
        """
        Initialize analyzer with configuration.
        
        Args:
            config_path: Path to the configuration file
        """
        self.config_path = config_path
        self.memory_regions: List[MemoryRegion] = []
        self.usage_data: Dict[str, float] = {}
    
    def analyze_memory_layout(self) -> Dict[str, float]:
        """
        Analyze the current memory layout and usage.
        
        Returns:
            Dictionary containing memory usage statistics
        """
        # Implementation
        pass
```

## 💬 Commit Message Guidelines

We follow the [Conventional Commits](https://www.conventionalcommits.org/) specification.

### Commit Message Format

```
<type>(<scope>): <subject>

<body>

<footer>
```

### Types:
- **feat**: New feature
- **fix**: Bug fix
- **docs**: Documentation changes
- **style**: Code style changes (formatting, etc.)
- **refactor**: Code refactoring
- **perf**: Performance improvements
- **test**: Adding/updating tests
- **chore**: Maintenance tasks
- **ci**: CI/CD changes
- **build**: Build system changes

### Examples:

```bash
# Good commit message
feat(npu): add memory pool allocation

- Implemented NPU memory pool with configurable size
- Added device tree overlay for NPU memory reservation
- Added test cases for memory allocation

Closes #123

# Another good example
fix(ddr): correct DDR timing configuration for Edge2 board

The previous timing values caused instability during boot.
Updated timings based on memory chip datasheet.

Fixes #456

# Bad commit message
fixed stuff
```

## 🧪 Testing Guidelines

### Unit Tests

```c
/* tests/unit_tests/test_ddr_layout.c */
#include "unity.h"
#include "ddr_layout.h"

void setUp(void) {
    /* Initialize test environment */
}

void tearDown(void) {
    /* Clean up after test */
}

void test_ddr_layout_init_success(void) {
    int ret = ddr_layout_init(BOARD_MIXTILE_EDGE2);
    TEST_ASSERT_EQUAL(0, ret);
}

void test_ddr_layout_init_invalid_board(void) {
    int ret = ddr_layout_init(BOARD_MAX);
    TEST_ASSERT_EQUAL(-EINVAL, ret);
}

void test_ddr_memory_allocation(void) {
    void *ptr = ddr_alloc(1024);
    TEST_ASSERT_NOT_NULL(ptr);
    ddr_free(ptr);
}
```

### Integration Tests

```bash
#!/bin/bash
# tests/integration_tests/test_full_boot.sh

echo "Running integration tests..."
echo "1. Testing boot with default config..."
./scripts/deployment/flash.sh --config default
sleep 10
if ! ddr_info_tool --status; then
    echo "ERROR: Boot test failed"
    exit 1
fi

echo "2. Testing NPU memory allocation..."
./examples/npu_memory_test
if [ $? -ne 0 ]; then
    echo "ERROR: NPU memory test failed"
    exit 1
fi

echo "All integration tests passed!"
```

### Running Tests

```bash
# Run all tests
./scripts/test/run_tests.sh --all

# Run unit tests only
./scripts/test/run_tests.sh --unit

# Run integration tests
./scripts/test/run_tests.sh --integration

# Run with coverage
./scripts/test/run_tests.sh --coverage
```

## 📚 Documentation Standards

### Code Documentation

```c
/**
 * ddr_memory_alloc - Allocate memory from DDR pool
 * @size: Number of bytes to allocate
 * @flags: Allocation flags (GFP_KERNEL, GFP_ATOMIC, etc.)
 * 
 * This function allocates memory from the DDR memory pool.
 * It ensures proper alignment and cache coherence.
 * 
 * Return: Pointer to allocated memory, or NULL on failure
 * 
 * Note: Memory must be freed with ddr_memory_free()
 * 
 * Example:
 *   void *ptr = ddr_memory_alloc(4096, GFP_KERNEL);
 *   if (ptr) {
 *       // Use memory
 *       ddr_memory_free(ptr);
 *   }
 */
void *ddr_memory_alloc(size_t size, gfp_t flags)
{
    /* Implementation */
}
```

### README Updates

When adding new features, update the README:

```markdown
## 🆕 New Feature: NPU Memory Management

### Overview
The NPU memory management system provides dedicated memory pools for neural processing unit operations.

### Usage
```c
#include <ddr_manager.h>

// Allocate NPU memory
void *npu_mem = ddr_npu_alloc(1024 * 1024);

// Use memory for AI inference
run_npu_inference(npu_mem, model);

// Free memory
ddr_npu_free(npu_mem);
```

### Configuration
```json
{
    "npu": {
        "memory_size": 128,
        "memory_type": "DMA",
        "cache_policy": "write_back"
    }
}
```
```

## 🔄 Pull Request Process

### PR Template

```markdown
## Description
[Clear description of what this PR does]

## Type of Change
- [ ] Bug fix
- [ ] New feature
- [ ] Documentation update
- [ ] Performance improvement
- [ ] Code refactoring
- [ ] Testing

## Checklist
- [ ] I have tested my changes
- [ ] I have updated the documentation
- [ ] I have added tests for new features
- [ ] I have followed the coding standards
- [ ] All existing tests pass

## Testing
[Describe how you tested your changes]

## Screenshots
[If applicable, add screenshots]

## Related Issues
Closes #(issue number)

## Additional Notes
[Any additional information]
```

### PR Review Process

1. **Two reviewers required** for code changes
2. **All tests must pass** before merging
3. **Documentation must be updated**
4. **No merge conflicts** with main branch
5. **CI/CD pipeline must pass**

## 🐛 Issue Reporting

### Bug Report Template

```markdown
## 🐛 Bug Report

### Description
[Clear description of the bug]

### Steps to Reproduce
1. [First step]
2. [Second step]

### Expected Behavior
[What should happen]

### Actual Behavior
[What actually happens]

### Environment
- Board: [e.g., Mixtile Edge 2]
- OS: [e.g., Ubuntu 20.04]
- Version: [e.g., 1.0.0]

### Logs/Screenshots
[Paste relevant logs or screenshots]

### Additional Context
[Any other information]
```

## 🤝 Community

### Communication Channels

- **GitHub Issues**: Bug reports and feature requests
- **GitHub Discussions**: Questions and community support
- **Discord/Slack**: Real-time chat (if applicable)

### Getting Help

If you need help with contributing, please:
1. Check the [documentation](https://github.com/yourusername/RK3568-DDR-Memory-Manager/docs)
2. Search [existing issues](https://github.com/yourusername/RK3568-DDR-Memory-Manager/issues)
3. Ask in [Discussions](https://github.com/yourusername/RK3568-DDR-Memory-Manager/discussions)

### Recognition

Contributors will be recognized in:
- [CONTRIBUTORS.md](CONTRIBUTORS.md) file
- Project README
- Release notes

