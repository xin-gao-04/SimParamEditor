# Repository Guidelines

## Project Structure & Module Organization
Source lives under `src/`, grouped by responsibility: the Qt entrypoint sits in `app/main.cpp`, reusable domain logic lives in `core/` (`param_types.h`, `json_io.*`, `validation.*`, `type_manager.*`), UI controllers live under `ui/main_window/` (split by top bar/property/instances/etc.), dialogs under `ui/dialogs/`, and code generation stays in `generator/`. Reusable assets (QSS, icons, templates) live in `resources/`, while CMake writes all build artifacts to `build/`.

## Build, Test, and Development Commands
- `cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt`: generate a Ninja/Makefiles tree targeting Qt 5.5+. Add `-DCMAKE_BUILD_TYPE=Debug` when iterating UI logic.
- `cmake --build build --config Release`: compile the Qt Widgets app; pass `--target SimParamEditor_autogen` if you only need to regenerate Qt UI glue.
- `./build/SimParamEditor` (or the `.app` on macOS): launch the editor with the current theme bundle.
- `ctest --test-dir build --output-on-failure`: run QtTest suites once they are registered; CI expects this to be clean before review.

## Coding Style & Naming Conventions
We target C++11 with Qt idioms: four-space indentation, opening braces on their own line, and `#include` order of Qt, STL, then local headers. Follow the established naming scheme from `README.md`: classes/types in PascalCase (`ParamModel`), functions and locals in camelCase, constants/macros in UPPER_SNAKE_CASE, and files in snake_case (`cpp_generator.cpp`). Favor Qt containers (`QString`, `QVector`) and avoid raw new/delete in favor of parented widgets. Before pushing, run `clang-format -style=file` if you introduce one-off formatting hotspots.

## Testing Guidelines
Automated coverage is still sparse; add Qt Test cases per module (e.g., `tst_validation.cpp`) under `src/tests/` and register them with `add_executable`/`add_test` so `ctest` can discover them. Name each test method after the scenario (`shouldRejectDuplicateNames`). Aim to cover boundary validation, type reuse in `TypeManager`, and JSON round-trips whenever you touch those areas. Pair automated checks with a quick manual smoke run: load an existing `.spe`, tweak nested structs, and generate C++ to confirm UI, validation, and codegen stay in sync.

## Commit & Pull Request Guidelines
The history mixes prefixes like `feat:` and `fix:` (see `git log --oneline`), so continue using a `<type>: <concise summary>` format; favor English summaries unless the change is domain-specific Chinese terminology. Break large refactors into logical commits with clear scope. Pull requests should describe the motivation, list user-visible changes (UI screenshots for theme work, code samples for generator updates), and link any tracking issues. Mention validation/test steps (`cmake --build`, `ctest`) so reviewers can reproduce results quickly. Avoid committing generated artifacts other than `compile_commands.json`.
