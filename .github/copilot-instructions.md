# Copilot Coding Agent Instructions for unordered_dense

## Project Overview

**unordered_dense** is a header-only C++17/20/23 library providing fast and densely stored hashmap and hashset implementations (`ankerl::unordered_dense::{map, set}`) based on robin-hood backward shift deletion. It is designed as a high-performance alternative to `std::unordered_map` and `std::unordered_set`.

- **Type**: Header-only C++ library
- **Languages**: C++17, C++20, C++23
- **Build Systems**: Meson (primary), CMake (for installation)
- **Size**: Small to medium (single main header: `include/ankerl/unordered_dense.h`)
- **Main Header**: `include/ankerl/unordered_dense.h`
- **Test Framework**: doctest
- **Benchmarking**: nanobench (custom framework)

## Build System & Commands

### Build System: Meson (Primary)

**CRITICAL**: This project uses **Meson** as its primary build system. Always use Meson commands for building and testing.

#### Prerequisites
```bash
pip install meson ninja  # Install from requirements.txt
```

#### Setup Build Directory

**ALWAYS** set up a build directory before building or testing. Common configurations:

```bash
# Debug build (C++17, default)
meson setup builddir/gcc_cpp17_debug

# Release build (C++17)
meson setup --buildtype=release builddir/gcc_cpp17_release

# C++20 build
meson setup -Dcpp_std=c++20 builddir/gcc_cpp20_debug

# C++23 build
meson setup -Dcpp_std=c++23 builddir/gcc_cpp23_debug

# 32-bit build
meson setup -Dcpp_args=-m32 -Dcpp_link_args=-m32 builddir/gcc_cpp17_debug_32

# Sanitizer builds
meson setup -Db_sanitize=address builddir/gcc_sanitize_address
meson setup -Db_sanitize=thread builddir/gcc_sanitize_thread
meson setup -Db_sanitize=undefined builddir/gcc_sanitize_undefined
```

**IMPORTANT**: The build directory name convention is `<compiler>_<standard>_<buildtype>` (e.g., `gcc_cpp17_release`).

#### Build Commands

```bash
# Compile (from project root)
meson compile -C builddir/gcc_cpp17_release

# Clean before build
meson compile --clean -C builddir/gcc_cpp17_release
meson compile -C builddir/gcc_cpp17_release
```

#### Test Commands

**ALWAYS run tests after making changes**. Tests are run with Meson:

```bash
# Run all tests (with error logs)
meson test -C builddir/gcc_cpp17_release -v

# Run tests quietly
meson test -C builddir/gcc_cpp17_release -q --print-errorlogs

# Run without rebuilding
meson test -C builddir/gcc_cpp17_release --no-rebuild -v
```

**Test location**: Test logs are written to `builddir/<config>/meson-logs/testlog.txt`

### CMake (Installation Only)

CMake is **only** used for installing the library, not for testing:

```bash
mkdir build && cd build
cmake ..
cmake --build . --target install
```

**Do NOT use CMake for building tests or running the test suite.**

## Linting & Code Quality

### Lint Scripts (MUST PASS)

All lint scripts are in `scripts/lint/`. They MUST pass before submitting changes:

```bash
# Run ALL linters (recommended)
./scripts/lint/all.py

# Individual linters
./scripts/lint/lint-version.py        # Check version consistency across files
./scripts/lint/lint-clang-tidy.py     # Run clang-tidy on main header
./scripts/lint/lint-clang-format.py   # Check code formatting
```

### Code Formatting

- **Tool**: clang-format
- **Config**: `.clang-format` (LLVM-based, 127 column limit)
- **Standard**: C++17
- **Style**: snake_case for all identifiers (classes, functions, variables)
- **Indentation**: 4 spaces, no tabs

**Key formatting rules**:
- Column limit: 127
- Pointer alignment: Left (`Type* ptr`)
- Break before commas in constructor initializers
- No short lambdas on single line

### Clang-Tidy

- **Config**: `.clang-tidy`
- **Header filter**: Only checks `unordered_dense.h`
- **Naming**: snake_case for everything, UPPER_CASE for macros
- **Command**: `./scripts/lint/lint-clang-tidy.py`

## CI/CD Pipeline (GitHub Actions)

**Workflow**: `.github/workflows/main.yml` runs on all pushes and PRs.

### CI Jobs

1. **lint** (Ubuntu): Runs `lint-version.py` and `lint-clang-tidy.py`
2. **mingw** (Windows): MinGW 32-bit and 64-bit builds
3. **windows** (Windows): MSVC 32-bit and 64-bit builds
4. **macos** (macOS): clang builds
5. **linux** (Ubuntu): gcc/clang × C++17/23 × 32/64-bit matrix
6. **linux-sanitizers** (Ubuntu): address/thread/undefined sanitizers

**CRITICAL**: All these jobs must pass. If you make changes:
- Ensure they work on gcc AND clang
- Test both C++17 and C++23 when relevant
- Consider 32-bit compatibility (avoid 64-bit assumptions)
- Sanitizers must pass (especially address/undefined)

### Common CI Failures & Fixes

1. **Linting failures**: Run `./scripts/lint/all.py` locally first
2. **32-bit failures**: Don't use `size_t` in hashes without consideration
3. **Sanitizer failures**: Check for undefined behavior, use-after-free, data races
4. **Windows/MSVC**: Check for MSVC-specific warnings (see `test/meson.build`)

## Project Structure

### Directory Layout

```
unordered_dense/
├── include/ankerl/unordered_dense.h   # Main header (ALL implementation)
├── src/ankerl.unordered_dense.cpp     # C++20 modules support
├── test/
│   ├── unit/          # Unit tests (doctest)
│   ├── bench/         # Benchmarks (nanobench)
│   ├── fuzz/          # Fuzz testing
│   ├── app/           # Test application code
│   └── meson.build    # Test build configuration
├── scripts/
│   ├── build.py       # Multi-configuration build script
│   └── lint/          # Linting scripts
├── .github/workflows/main.yml  # CI configuration
├── meson.build        # Main build file
├── CMakeLists.txt     # Installation only
├── .clang-format      # Format configuration
└── .clang-tidy        # Linter configuration
```

### Key Files to Know

- **`include/ankerl/unordered_dense.h`**: The entire library (header-only)
- **`test/unit/`**: All unit tests (named by feature, e.g., `insert.cpp`, `erase.cpp`)
- **`test/bench/`**: Performance benchmarks
- **`test/meson.build`**: Test compilation settings, dependencies, warning flags
- **`scripts/build.py`**: Local multi-configuration build/test script

### Configuration Files

- **`.clang-format`**: Code formatting rules (use with clang-format)
- **`.clang-tidy`**: Static analysis rules
- **`.gitignore`**: Excludes builddir, .cache, .vscode, compile_commands.json
- **`requirements.txt`**: Python dependencies (meson, ninja)

## Development Workflow

### Making Changes

1. **Setup**: Ensure you have a build directory set up
   ```bash
   meson setup builddir/gcc_cpp17_debug
   ```

2. **Edit**: Make changes to `include/ankerl/unordered_dense.h` or test files

3. **Build**: Compile the tests
   ```bash
   meson compile -C builddir/gcc_cpp17_debug
   ```

4. **Test**: Run all tests
   ```bash
   meson test -C builddir/gcc_cpp17_debug -v
   ```

5. **Lint**: Verify all linters pass
   ```bash
   ./scripts/lint/all.py
   ```

6. **Multi-config** (optional): Test across configurations
   ```bash
   python scripts/build.py  # Builds and tests many configurations
   ```

### Common Tasks

#### Adding a New Feature
1. Implement in `include/ankerl/unordered_dense.h`
2. Add unit test in `test/unit/<feature>.cpp`
3. Update `test/meson.build` to include new test file
4. Build and test
5. Run linters

#### Fixing a Bug
1. Add a failing test case in appropriate `test/unit/*.cpp` file
2. Fix the bug in `include/ankerl/unordered_dense.h`
3. Verify test passes
4. Run full test suite and linters

#### Updating Version
- Version must be updated in 4 places (checked by `lint-version.py`):
  - `meson.build` (version: 'X.Y.Z')
  - `CMakeLists.txt` (VERSION X.Y.Z)
  - `include/ankerl/unordered_dense.h` (ANKERL_UNORDERED_DENSE_VERSION_*)
  - `test/unit/namespace.cpp` (unordered_dense::vX_Y_Z)

## Dependencies

### Build Dependencies
- **meson** (1.7.2+): Build system
- **ninja**: Build backend
- **C++17 compiler**: gcc or clang (C++20/23 for advanced features)

### Test Dependencies (auto-fetched by Meson)
- **doctest** (2.4.11+): Unit testing framework
- **fmt** (11.2.0+): Formatting library
- **boost** (optional): For custom container tests

### Optional Dependencies
- **clang-tidy**: For static analysis
- **clang-format**: For code formatting
- **ccache**: For faster rebuilds (recommended)

## Important Notes & Gotchas

### Build System
- **NEVER** use CMake for testing - it's only for installation
- **ALWAYS** use Meson for building and testing
- Build directories follow naming: `<compiler>_<standard>_<type>`
- Tests output to `builddir/<config>/meson-logs/testlog.txt`

### Code Style
- This is a **header-only** library - all code in `unordered_dense.h`
- Use **snake_case** for everything (enforced by clang-tidy)
- No magic numbers (use named constants)
- Maintain C++17 compatibility unless explicitly C++20/23 features

### Testing
- **ALWAYS** run `meson test` after changes
- Tests use doctest with `[ts=...]` tags for categorization
- Benchmarks are also tests (run with `-ts=bench`)
- Fuzz tests exist but aren't run in standard test suite

### Platform Considerations
- Code must work on Windows (MSVC), Linux (gcc/clang), macOS (clang)
- Support both 32-bit and 64-bit architectures
- Be careful with integer sizes (use appropriate types)
- Sanitizers must pass (especially address and undefined behavior)

### Version Control
- Versions are checked across 4 files - update all or lint will fail
- CI runs on every push/PR - ensure it passes locally first

## Trust These Instructions

**IMPORTANT**: These instructions are comprehensive and tested. Trust them and follow the documented commands. Only explore or search for alternatives if:
- A documented command fails with an unexpected error
- You need information not covered here
- The repository structure has changed significantly

When in doubt, run `./scripts/lint/all.py` and `meson test -C builddir/<config> -v` to verify your changes.
