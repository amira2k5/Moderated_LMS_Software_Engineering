# Moderated Learning Management System (LMS)

A console application for a secondary school. Lecturers create assignments and mark work, students
submit and resubmit, heads of department moderate and release marks, and an administrator manages
accounts and courses. Students never see a mark until the head of department has checked and released it.

* Language: C++17 (CMake build). Storage: SQLite (a single local file). Works offline.
* Passwords: salted Argon2id hashes (libsodium). Every mark change is recorded in an append-only audit log.
* Late work is accepted: 10% of the assignment's maximum mark is lost for each started 24 hours, for up to
  three days; after that the mark is zero.

## Build guide

Requirements: a C++17 compiler (GCC 9+ or Clang 10+), CMake 3.16+, SQLite 3 (3.24+) and libsodium
development packages. The tests use GoogleTest (downloaded automatically if it is not installed).

Ubuntu or Debian:

    sudo apt-get install g++ cmake libsqlite3-dev libsodium-dev libgtest-dev
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build
    ctest --test-dir build --output-on-failure     # runs all automated tests
    ./build/lms --db lms.db --files lms_files      # starts the program

On first start the program asks you to create the first administrator account.
Options: `--db FILE` (database file, default `lms.db`), `--files FOLDER` (where submitted files are
kept, default `lms_files`), `--version`, `--help`.

Coverage report (needs gcovr):

    cmake -S . -B build-cov -DCMAKE_BUILD_TYPE=Debug -DLMS_COVERAGE=ON
    cmake --build build-cov && ./build-cov/lms_tests
    gcovr -r . build-cov --filter src/domain.cpp --filter src/services.cpp --filter src/common.cpp --txt

## Repository layout

| Path | Contents |
|---|---|
| `src/common.*` | Time, error types, Clock |
| `src/domain.*` | User hierarchy, entities, late-penalty Strategy, mark-change Observer, UserFactory |
| `src/data.*`, `src/schema.cpp` | SQLite wrapper, unit of work, repository interfaces and SQLite implementations |
| `src/security.*` | Password hasher interface and libsodium implementation |
| `src/services.*` | Application services (one per group of use cases) |
| `src/ui.*`, `src/main.cpp` | Console menus and program entry point |
| `tests/` | GoogleTest unit, integration, system and performance tests |
| `schema/schema.sql` | The database schema (also embedded in `src/schema.cpp`) |
| `docs/` | Installation guide, user manual, release notes, test and quality reports |

See `CODING_STANDARD.md` and `CONTRIBUTING.md` for how to work on the code.
