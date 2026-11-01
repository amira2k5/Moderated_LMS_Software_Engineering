# Quality assurance report (v1.1.0)

## 1. Static analysis
Tool: **cppcheck 2.13.0** with `--enable=warning,style,performance,portability --inconclusive --std=c++17` on `src/` and `tests/`.
Compiler: GCC 13.3 with `-Wall -Wextra -Wpedantic`: no warnings in either version.

| Run | Result |
|---|---|
| First run on the working code | Findings in five groups (see DEF-06): non-explicit constructor; parameter names differing between declaration and definition; raw loops where a standard algorithm is clearer; initialisation in a constructor body (tests); unused variable (tests) |
| After fixes (v1.0.0 and v1.1.0) | **0 findings** in `src/` and `tests/` |

The CI job `quality` fails the build if cppcheck reports anything for `src/`.

## 2. Code metrics (lizard 1.24.3)

| Metric | v1.0.0 | v1.1.0 |
|---|---|---|
| Source files in `src/` | 14 (8 `.cpp`, 6 `.hpp`) | same |
| Non-comment lines of code in `src/` (NLOC) | 2,263 | 2,304 |
| Total lines in `src/` (including blank and comment lines) | 2,494 | 2,537 |
| Total lines in `tests/` | 1,342 | 1,511 |
| Functions | 212 | 213 |
| Average NLOC per function | 7.0 | 7.1 |
| Average cyclomatic complexity (CCN) | 2.4 | 2.5 |
| Functions with CCN above 10 | 2 | 2 |
| Line coverage, business-logic layer | 98.5% | 98.6% |

Key functions:

| Function | NLOC | CCN | Comment |
|---|---|---|---|
| `SqliteAuditRepository::list` | 30 | 13 | One `if` per optional filter; each adds a fixed `?` fragment. Accepted: flat and fully tested. |
| `main` | 46 | 11 | Command-line options and wiring of the concrete classes. Accepted: only place where classes are created. |
| `AdminService::createUser` | 20 | 8 | After extracting `validateUsername`; it was above 10 before the review. |
| `MarkingService::mark` | 23 | 5 | Central business rule, all branches tested. |
| `TenPercentPerDayPolicy::calculate` | 11 | 3 | The late-penalty rule. |
| Role menus (`adminMenu`, `lecturerMenu`, `hodMenu`, `studentMenu`) | 19 to 45 | 1 to 6 | Table-driven; the admin menu was CCN 23 before the review. |

## 3. Self code review (checklist) and changes made

| # | Checklist question | Finding | Change |
|---|---|---|---|
| 1 | Is any SQL built from user input? | No. All 40 statements in `data.cpp` are prepared and bound; the audit filter adds only fixed fragments with `?`. | None needed. |
| 2 | Is SQL used outside the data layer? | No (searched `src/` for `exec(` and `prepare(`). | None. |
| 3 | Can a password be logged or stored in clear text? | No: only `IPasswordHasher` sees it; the database holds Argon2id hashes (test). | None. |
| 4 | Does every service method check the role first? | Yes; `requireAllowed` is the first line of each public method (tests in `AccessControl`). | None. |
| 5 | Are multi-step changes atomic? | Yes: approve, release, class extension and marking use one transaction (tests roll back). | None. |
| 6 | Are resources released on errors (RAII)? | Statements, transactions and file copies are released in destructors or `catch` blocks. | None needed: the stored copy is removed when the database write fails (test `Submission.DatabaseFailureLeavesNoPartialSubmission`). |
| 7 | Are error messages written for school users? | Reviewed all messages. Database errors still include SQLite's own text after a plain first sentence. | None; the plain sentence comes first. A later improvement could hide the technical text. |
| 8 | Are functions small and simple? | Seven functions were above CCN 10 (DEF-04). | Table-driven menus; `validateUsername` and `checkedFileSize` extracted. |
| 9 | Are names and parameters consistent between header and source? | Several mismatches found by cppcheck. | Renamed parameters. |
| 10 | Do constructors with one argument avoid accidental conversion? | `TenPercentPerDayPolicy` did not. | Made it `explicit`. |
| 11 | Does the code follow the design? | Four deliberate differences, listed in section 4. | Design document updated. |
| 12 | Is there dead or duplicated code? | Two near-identical list loops in the menus. | Merged into `lecturerCourses`. |

## 4. Fidelity to the design (differences from the Phase 3 design)

| Design said | Code does | Reason |
|---|---|---|
| `AssignmentService` uses assignment and course repositories | Also uses `IMarkRepository` and `IUserRepository` (and `IUnitOfWork` since CR1) | It lists submissions and a course's students (DEF-01); CR1 needs a transaction |
| `AdminService` backs up "through the SQLite backup interface" | Holds a `Database&` for `backupTo()` | The backup API works on the database connection |
| An RAII `Transaction` | `IUnitOfWork` with `SqliteUnitOfWork` and nesting | Lets services be tested and nested calls stay atomic |
| `FileStore` | `IFileStore` and `LocalFileStore` | Interface name follows the other abstractions |

The database matches the class diagram and the Phase 3 schema: `src/schema.cpp` is the same schema, and the tests exercise every table.
