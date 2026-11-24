# Building and running the Catch2/CTest suite

## 1. Pattern & Key Files

- `run_tests.bat` (repo root): sets up the same MSVC x64 environment as `build_ninja.bat`, builds **every** target (`rme` + all test executables) with Ninja and runs the full CTest suite. Exits non-zero on any compile error or test failure.
- `build_ninja.bat`: configures the build directory and builds only the `rme` target; it does not compile tests.
- Test targets and `add_test()` registrations live in the root `CMakeLists.txt`; full-editor test executables share the `rme_add_full_editor_test()` CMake function, and every standalone Catch2 suite registers through the one-line `rme_add_catch2_test(target sources...)` helper (C++23, Catch2WithMain, `source/` include, CTest entry) instead of a hand-copied block - new suites add exactly one line.

## 2. When to Use It (Do's & Don'ts)

- Run `run_tests.bat` after any change that touches compiled code, before reporting a task as done. The full-editor test targets link the whole `source/` tree (minus `application.cpp`), so a signature change breaks them even when `rme.exe` still builds.
- Do not call `ninja` or `ctest` directly from a plain shell: the MSVC environment variables (STL include paths) are only set by the script's `vcvarsall` step, and `cl.exe` will fail with `C1083: cannot open include: 'vector'`.
- Run one suite by name afterwards: `ctest --test-dir build-ninja -R magic_wand --output-on-failure`.
- The script requires an existing `build-ninja\build.ninja`; run `build_ninja.bat` once if it is missing.

## 3. Minimal Usage

```bat
:: from the repo root
run_tests.bat                 :: build everything, then run all 14 CTest suites
ctest --test-dir build-ninja -R magic_wand --output-on-failure   :: re-run just the wand suites
```

## 4. Invariants & Pitfalls

- `cmake --build` there builds **all** targets: keep it that way instead of listing test names, so new suites are picked up automatically.
- The `test_magic_wand_plan` target uses a precompiled header; a build aborted mid-compile can leave a stale PCH behind. `build_ninja.bat clean` clears it.
- Tests must stay green on Windows MSVC and on the Linux CI; do not add platform-specific switches to the script.
