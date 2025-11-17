# NDP-SIM: Event-Driven Near-Data Processing Simulator (Bazel Build)

## Overview

NDP-SIM is an event-driven simulation framework for near-data processing architectures. This project has been migrated from CMake to **Bazel** for improved build scalability, reproducibility, and dependency management.

## Prerequisites

- **Bazel 6.0+** - [Install Bazel](https://bazel.build/install)
- **C++20 compatible compiler** (clang, gcc, or MSVC)
- **macOS, Linux, or Windows** (with Bazel support)

## Quick Start

### Build the main simulator:
```bash
bazel build //:main
```

### Run all tests:
```bash
bazel test //tests:all
```

### Build and run a specific test:
```bash
bazel run //tests:test_alu
bazel run //tests:test_pipeline
bazel run //tests:test_core_rvv_integration
```

### View build outputs:
Compiled binaries are in `bazel-bin/`:
```bash
./bazel-bin/main                    # Main simulator
./bazel-bin/tests/test_alu          # Individual test executable
```

## Project Structure

```
.
├── BUILD                           # Root Bazel build file
├── WORKSPACE                       # Bazel workspace configuration
├── .bazelrc                        # Bazel configuration options
├── .bazelignore                    # Files/directories to ignore during builds
├── src/
│   ├── BUILD                       # Bazel build for core components
│   ├── *.h                         # Core architecture headers
│   ├── npu.cpp                     # Main simulator implementation
│   ├── comp/
│   │   ├── BUILD                   # Component libraries (ALU, LSU, TPU, etc.)
│   │   ├── core/                   # Core processor components
│   │   ├── rvv/                    # RVV vector extension components
│   │   └── *.h                     # Component headers
│   └── conn/
│       ├── BUILD                   # Connection libraries
│       └── *.h                     # Connection primitives (ready-valid, credit, etc.)
├── tests/
│   ├── BUILD                       # Test targets
│   └── test_*.cpp                  # GoogleTest test suites
├── third_party/                    # External dependencies
│   ├── googletest/                 # Google Test framework
│   └── DRAMsim3/                   # DRAM simulator
├── config/                         # Configuration generators
├── bitstream/                      # Bitstream utilities
└── README.md                       # This file
```

## Building with Bazel

### Basic Commands

**Configure and build:**
```bash
# Build the main target
bazel build //:main

# Build all targets in tests directory
bazel build //tests:all

# Build with specific configuration
bazel build --config=clang //:main
```

**Run tests:**
```bash
# Run all tests
bazel test //tests:all

# Run specific test with verbose output
bazel test //tests:test_alu --test_output=all

# Run tests matching a pattern
bazel test //tests:test_rvv_* --test_output=short
```

**Clean build artifacts:**
```bash
bazel clean
```

### Build Options

The `.bazelrc` file provides predefined configurations:

```bash
# Default options applied:
# - C++20 standard
# - Compiler warnings enabled (-Wall, -Wextra, -Wpedantic)
# - Optimization level -O2
# - Short test output format

# Override settings:
bazel build --cxxopt=-O3 //:main          # Higher optimization
bazel test --test_output=all //tests:all  # Verbose test output
```

## Supported Tests

The project includes comprehensive unit tests for all major components:

| Test | Coverage |
|------|----------|
| `test_alu` | Arithmetic Logic Unit functionality |
| `test_fpu` | Floating Point Unit operations |
| `test_pipeline` | Pipeline hazards and stalls |
| `test_lsu` | Load-Store Unit memory operations |
| `test_credit_connection` | Credit-based flow control |
| `test_mlu` | Matrix-Logic Unit |
| `test_dvu` | Data Vector Unit |
| `test_bru` | Branch Resolution Unit |
| `test_regfile` | Register file operations |
| `test_core` | Core processor functionality |
| `test_core_dispatch` | Instruction dispatch logic |
| `test_core_eventdriven` | Event-driven scheduling |
| `test_rvv_alu` | RVV vector ALU |
| `test_rvv_dvu` | RVV vector DVU |
| `test_rvv_regfile` | RVV register file |
| `test_rvv_dispatch` | RVV dispatch unit |
| `test_rvv_rob` | RVV reorder buffer |
| `test_rvv_retire` | RVV retire stage |
| `test_rvv_backend` | RVV backend integration |
| `test_core_rvv_integration` | Core + RVV integration |

## Architecture Highlights

### Layered Simulation Stack
```
┌─────────────────────────────────────────────┐
│ Application / Operator Layer                 │
├─────────────────────────────────────────────┤
│ Accelerator Primitives (ALU, LSU, TPU, PE)  │
├─────────────────────────────────────────────┤
│ Communication Fabric (Ports, Connections)   │
├─────────────────────────────────────────────┤
│ Event-Driven Kernel (Scheduler)              │
├─────────────────────────────────────────────┤
│ Configuration & Bitstream Toolchain          │
└─────────────────────────────────────────────┘
```

### Key Components

- **Event-Driven Kernel** (`src/event.h`, `src/scheduler.h`): Cycle-accurate simulation with deterministic event scheduling
- **Components** (`src/component.h`, `src/tick.h`): Ticking components, pipelined execution, and lifecycle management
- **Communication Fabric** (`src/port.h`, `src/connection.h`, `src/conn/`): Ready-valid handshakes, credit-based flow control
- **Processing Units** (`src/comp/`):
  - Pipelined ALU, FPU
  - Load-Store Unit (LSU)
  - Systolic Array TPU with MAC grid
  - RVV vector extensions
- **Memory Subsystem** (`src/comp/dram.h`): SRAM and DRAM simulation

## Configuration

Edit `.bazelrc` to customize build settings:

```bash
# Compiler options
build --cxxopt=-std=c++20
build --cxxopt=-O2

# Test settings
test --test_output=short
test --verbose_failures

# Use clang compiler (uncomment to enable)
# build:clang --compiler=clang
```

## Dependencies

Bazel automatically manages dependencies via the `WORKSPACE` file:

- **Google Test** (com_google_googletest): Unit testing framework
- **Rules CC** (rules_cc): C/C++ build rules

Third-party libraries vendored in `third_party/`:
- **DRAMsim3**: Cycle-accurate DRAM simulator
- **GoogleTest**: Already handled via Bazel

## Troubleshooting

### Build fails with "compiler not found"
Ensure a C++20 compatible compiler is installed:
```bash
# macOS
brew install llvm

# Ubuntu/Debian
sudo apt-get install build-essential

# CentOS/RHEL
sudo yum install gcc-c++
```

### Tests fail to link
Verify pthread is available:
```bash
bazel test //tests:test_alu --verbose_failures
```

### Clean rebuild needed
```bash
bazel clean --expunge
bazel build //:main
```

## Development Workflow

### Adding a new test:
1. Create `tests/test_new_component.cpp`
2. Add a `cc_test` target in `tests/BUILD`
3. Run: `bazel test //tests:test_new_component`

### Adding a new component:
1. Create headers in `src/comp/`
2. Update `src/comp/BUILD` with new `cc_library` target
3. Add dependency to `src/BUILD`
4. Rebuild: `bazel build //:main`

## Key Advantages of Bazel

- **Reproducible builds**: Hermetic, deterministic compilation
- **Fast incremental builds**: Only rebuilds what changed
- **Scalability**: Handles large projects efficiently
- **Remote execution**: Distributable build jobs
- **Better dependency management**: Explicit dependency graph

## Next Steps

- Explore individual test failures with `bazel test --test_output=all //tests:test_name`
- Build optimized release binary: `bazel build -c opt //:main`
- Check build dependencies: `bazel query "deps(//:main)"`

## References

- [Bazel Documentation](https://bazel.build/docs)
- [Bazel C++ Guide](https://bazel.build/docs/cpp)
- [GoogleTest Documentation](https://github.com/google/googletest)
