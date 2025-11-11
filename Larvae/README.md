# Larvae - HiveEngine Testing Framework

**Larvae** is HiveEngine's unit testing framework.

## Features

- **Lightweight**: No heavy external dependencies (Google Test, Catch2, etc.)
- **Auto-registration**: Tests register themselves automatically
- **Fixtures**: Support for setup/teardown
- **Filtering**: Run subsets of tests with wildcards
- **Benchmarking**: Integrated performance testing with statistics
- **Cross-platform**: Works on Windows (MSVC), Linux (GCC), macOS (Clang)
- **Integrated**: Designed specifically for HiveEngine

## Quick Start

### Declare a Simple Test

```cpp
#include <larvae/larvae.h>

static auto test1 = larvae::RegisterTest("MathSuite", "AdditionWorks", []() {
    int result = 2 + 3;
    larvae::AssertEqual(result, 5);
});
```

### Test with Fixture

```cpp
class AllocatorFixture : public larvae::TestFixture
{
protected:
    void SetUp() override
    {
        allocator = new MyAllocator{1024};
    }

    void TearDown() override
    {
        delete allocator;
    }

    MyAllocator* allocator = nullptr;
};

static auto test2 = larvae::RegisterTestWithFixture<AllocatorFixture>(
    "AllocatorSuite", "AllocateWorks",
    [](AllocatorFixture& fixture) {
        void* ptr = fixture.allocator->Allocate(64, 8);
        larvae::AssertNotNull(ptr);
    });
```

### Available Assertions

```cpp
// Equality
larvae::AssertEqual(actual, expected)
larvae::AssertNotEqual(actual, expected)

// Comparisons
larvae::AssertLessThan(a, b)
larvae::AssertLessEqual(a, b)
larvae::AssertGreaterThan(a, b)
larvae::AssertGreaterEqual(a, b)

// Booleans
larvae::AssertTrue(condition)
larvae::AssertFalse(condition)

// Pointers
larvae::AssertNull(ptr)
larvae::AssertNotNull(ptr)

// Floats with epsilon
larvae::AssertNear(a, b, epsilon)
larvae::AssertFloatEqual(a, b)      // epsilon = 1e-5f
larvae::AssertDoubleEqual(a, b)     // epsilon = 1e-9

// Strings (accepts string_view, const char*, std::string)
larvae::AssertStringEqual(str1, str2)
larvae::AssertStringNotEqual(str1, str2)
```

## Running Tests

### Build

```bash
# Generate project
cmake -B build

# Build
cmake --build build

# Tests are in build/bin/larvae_runner
```

### Run

```bash
# All tests
./build/bin/larvae_runner

# Filter by suite
./build/bin/larvae_runner --suite=Comb

# Filter by pattern (supports wildcards)
./build/bin/larvae_runner --filter=*Allocator*

# Verbose mode (show full error messages)
./build/bin/larvae_runner --verbose

# Repeat N times
./build/bin/larvae_runner --repeat=10

# Random order
./build/bin/larvae_runner --shuffle

# Stop at first failure
./build/bin/larvae_runner --stop-on-failure
```

## Project Structure

```
Larvae/
├── include/larvae/
│   ├── larvae.h           # Main header
│   ├── test_registry.h    # Test registration
│   ├── test_runner.h      # Test executor
│   ├── assertions.h       # Assertion functions
│   ├── fixture.h          # Fixture support
│   ├── test_info.h        # Test information
│   └── test_result.h      # Test result
└── src/larvae/
    ├── test_registry.cpp
    ├── test_runner.cpp
    ├── assertions.cpp
    └── larvae.cpp

Brood/
├── main.cpp               # Entry point
├── memory/
│   └── test_allocator.cpp # Tests for Hive memory
└── CMakeLists.txt
```

## Adding Tests for a Module

1. Create a file in `Brood/module_name/`
2. Include `<larvae/larvae.h>`
3. Use `RegisterTest()` or `RegisterTestWithFixture<>()`
4. Add the file to `Brood/CMakeLists.txt`

Example:

```cpp
// Brood/mymodule/test_myclass.cpp
#include <larvae/larvae.h>
#include <mymodule/myclass.h>

static auto test_basic = larvae::RegisterTest("MyModule", "BasicTest", []() {
    MyClass obj;
    larvae::AssertTrue(obj.IsValid());
});
```

## CI/CD Integration

Larvae returns standard exit codes:
- `0`: All tests pass
- `1`: At least one test fails

Example GitHub Actions:

```yaml
- name: Run tests
  run: ./build/bin/larvae_runner
```

All assertions throw `AssertionFailedException` with detailed error messages including file location, expected/actual values, and custom context.

## License

Larvae follows the same license as HiveEngine.
