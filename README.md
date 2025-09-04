# NDP Simulator

This project implements a simple Processing Element (PE) simulator in C++, with input/output buffers and a pipelined ALU. It also includes unit tests using GoogleTest.

## CMake Build Process

1. **Create build directory and configure:**

```bash
cmake -S . -B build
````

This will configure the project and fetch GoogleTest automatically.

2. **Build the project:**

```bash
cmake --build build
```

This will generate:

* `build/ndp_sim` → executable for simulation (from `main.cpp`)
* `build/pe_tests` → GoogleTest executable

3. **Run the simulation demo:**

```bash
./build/ndp_sim
```

---

## GoogleTest Unit Testing

1. **Install GoogleTest**

### Ubuntu / Debian

```bash
sudo apt update
sudo apt install libgtest-dev cmake
cd /usr/src/gtest
sudo cmake .
sudo make
sudo cp *.a /usr/lib
```

### macOS (Homebrew)

```bash
brew install googletest
```

2. **Build the tests**

```bash
cmake -S . -B build
cmake --build build
```

3. **Run all tests via `ctest`:**

```bash
ctest --test-dir build
```

4. **Or run the test executable directly:**

```bash
./build/pe_tests
```

---

## C++ Coding Guidelines

* **Format:** Google C++ Style

* **Best Practices References:**

  * *Effective C++*
  * *More Effective C++*
  * *Effective Modern C++*

---

## Notes

* Unit tests ensure that the pipeline correctly handles ALU latency and maintains the correct output sequence.

* GoogleTest is fetched automatically via CMake `FetchContent`.
