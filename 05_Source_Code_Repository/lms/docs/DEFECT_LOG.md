# Defect log

Severity: High (wrong or lost data, security), Medium (feature unusable or hard to maintain), Low (cosmetic, test or build only).

| ID | Found in | Severity | Description | Cause | Fix | Retest |
|---|---|---|---|---|---|---|
| DEF-01 | Implementation (console UI) | Medium | A lecturer could not choose a student for an extension: no operation listed the students of a course. | Design gap: the use case step "select student" had no supporting operation in `AssignmentService`. | Added `IUserRepository` to `AssignmentService` and the operation `studentsIn()`; design document updated. | AssignmentSvc.ListingsRespectRoles passes; console extension flow works. |
| DEF-02 | Unit tests | Low | Boundary test expected 31 days late for exactly 30 days. | Wrong expectation in the test (30 x 24 h is 30 started periods). | Corrected the expected value. | Penalty.BoundaryValuesForDeadlineAndDays passes. |
| DEF-03 | Build of tests | Low | Test file would not compile (`std::nan` not declared). | Missing `#include <cmath>`. | Added the include. | Build clean. |
| DEF-04 | Static analysis and metrics | Medium | Seven functions were above the coding-standard limit of 10 cyclomatic complexity; the console menu functions were the worst (up to 23). | Large `switch` statements inside the role menus; long validation sequences. | Table-driven menus (one small function per menu line); validators extracted from `createUser` and `LocalFileStore::store`. | All 93 tests pass; 2 functions remain above 10 (13 and 11), accepted. |
| DEF-05 | Regression test for CR1 | Medium | Console test `LecturerMarksAndHodModeratesAndReleases` failed after CR1: choosing 5 no longer marked a submission. | The new menu item was inserted at position 4, which renumbered existing options and would have confused users and the manual. | Moved the new item to position 7; "Change my password" became 8. | 93 of 93 regression tests pass. |
| DEF-06 | Static analysis (cppcheck) | Low | Style findings in five groups: non-explicit constructor, parameter names differing between declaration and definition, raw loops that should use standard algorithms, constructor-body initialisation and an unused variable in tests. | Coding habits. | Fixed all; cppcheck now reports nothing for `src/` and `tests/`. | cppcheck clean; tests pass. |
| DEF-07 | Verification review | Low | NFR-10 (five simultaneous sessions) had no automated test at v1.0. | Verification gap, not a software fault. | Added `Concurrency.FiveSessionsWriteToOneDatabaseFileWithoutCorruption`. | Passes (run three times). |

No defect of high severity was found. Open items are listed in `docs/TEST_REPORT.md` section 8.
