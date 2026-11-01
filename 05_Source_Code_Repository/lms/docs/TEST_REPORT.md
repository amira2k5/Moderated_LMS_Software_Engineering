# Test report (v1.1.0)

## 1. Test strategy (V-Model)

| Level | Verifies | Test suites | Entry criteria | Exit criteria |
|---|---|---|---|---|
| Unit | Detailed design: penalty policy, permissions, factory, time helpers | Penalty, Domain, Common | Code compiles with no warnings | All pass; business-logic line coverage at least 70% |
| Integration | Architecture: services working with the SQLite repositories, unit of work, hasher, file store | Data, Auth, Admin, AssignmentSvc, Submission, Marking, Moderation, Concurrency | Unit tests pass | All pass; no open defect of high severity |
| System | SRS and use case specifications, including exception flows | UC18, UC10, UC14, AccessControl, Console, Robustness, Performance | Integration tests pass | Every flow of the three detailed use cases has a test; all pass; NFR targets met |
| Acceptance | Stakeholder statements (validation) | Scenarios AT-01 to AT-06 below | System tests pass | Each stakeholder concern demonstrated, or listed as not yet verified |
| Regression (CR1) | Existing behaviour after the change | All 93 v1.0 tests | CR1 code complete | All 93 pass unchanged |

Techniques: **equivalence partitioning** (valid and invalid users, roles, states, files) and **boundary value analysis** for deadlines (1 s before, at, 1 s after the deadline; exactly 24 h, 24 h + 1 s, 72 h, 72 h + 1 s late), marks (-0.5, 0, maximum, maximum + 0.01), file size (0, 5 MB, 5 MB + 1 byte), username length (2, 33), maximum mark (0, 1, 1000, 1001) and password rules.

## 2. Results by suite

| Suite | Level | Tests | Passed |
|---|---|---|---|
| Penalty | Unit | 4 | 4 |
| Domain | Unit | 4 | 4 |
| Common | Unit | 1 | 1 |
| Data | Integration | 9 | 9 |
| Auth | Integration | 7 | 7 |
| Admin | Integration | 3 | 3 |
| AssignmentSvc | Integration | 3 | 3 |
| Submission | Integration | 8 | 8 |
| Marking | Integration | 7 | 7 |
| Moderation | Integration | 8 | 8 |
| Robustness | System | 2 | 2 |
| Performance | System | 1 | 1 |
| Console | System | 9 | 9 |
| UC18 | System | 9 | 9 |
| UC10 | System | 8 | 8 |
| UC14 | System | 8 | 8 |
| AccessControl | System | 2 | 2 |
| CR1 | Maintenance (CR1) | 10 | 10 |
| Concurrency | Integration | 1 | 1 |
| **Total** | | **104** | **104** |

Full list: `docs/TEST_RESULTS.md`. Every functional requirement is traced to tests in `docs/TRACEABILITY.md`.

## 3. Coverage (gcov, line coverage)

| Scope | v1.0.0 | v1.1.0 |
|---|---|---|
| Business-logic layer (domain, services, common) | 98.5% (523 of 531 lines) | **98.6% (546 of 554 lines)** |
| All source except `main.cpp` | 92.3% (1203 of 1303) | 92.5% (1237 of 1337) |
| Target (SRS NFR-11) | at least 70% | at least 70% |

Lines not covered in the business-logic layer are defensive branches (for example the unreachable `return` after an exhaustive `switch`). The console layer (`ui.cpp`) is 69% covered because some password-hiding and terminal code paths need a real terminal.

## 4. Performance (NFR-01, NFR-02) with 800 students and 60 staff stored

| Measurement | Target | v1.0.0 | v1.1.0 |
|---|---|---|---|
| Login (Argon2id, production strength) | 2,000 ms | 81 ms | 78 ms |
| List 150 submissions | 1,000 ms | 0.5 ms | 0.5 ms |
| Save a mark (with audit entry) | 1,000 ms | 1.9 ms | 2.4 ms |
| Class extension for 800 students (CR1) | 1,000 ms | n/a | 32 ms |

Measured on the development computer; the school's older computers may be several times slower, which still leaves a wide margin.

## 5. System tests derived from the use cases

| ID | Use case flow | Test |
|---|---|---|
| ST-01 | UC18 main | UC18.Main_OnTimeSubmission (PASS) |
| ST-02 | UC18 A1 resubmission | UC18.A1_ResubmissionBeforeDeadline (PASS) |
| ST-03 | UC18 A2 late | UC18.A2_LateSubmissionIsAcceptedAndFlagged (PASS) |
| ST-04 | UC18 A2 over 72 h | UC18.A2_SubmissionAfterSeventyTwoHoursIsStillAccepted (PASS) |
| ST-05 | UC18 E1 not enrolled | UC18.E1_StudentNotEnrolled (PASS) |
| ST-06 | UC18 E2 invalid file | UC18.E2_InvalidFile (PASS) |
| ST-07 | UC18 E3 late work resubmit | UC18.E3_ResubmissionOfLateWorkRefused (PASS) |
| ST-08 | UC18 E4 after marking | UC18.E4_SubmissionAfterMarkingRefused (PASS) |
| ST-09 | UC18 E5 storage error | UC18.E5_StorageErrorLeavesNoPartialVersion (PASS) |
| ST-10 | UC10 main | UC10.Main_MarkAfterDeadlineHiddenFromStudent (PASS) |
| ST-11 | UC10 late penalty | UC10.Main_LatePenaltyShownAndApplied (PASS) |
| ST-12 | UC10 A1 re-mark | UC10.A1_RemarkWithReason (PASS) |
| ST-13 | UC10 E1 range | UC10.E1_MarkOutsideRange (PASS) |
| ST-14 | UC10 E2 feedback | UC10.E2_FeedbackMissing (PASS) |
| ST-15 | UC10 E3 deadline | UC10.E3_DeadlineNotYetPassed (PASS) |
| ST-16 | UC10 E4 moderated | UC10.E4_ModeratedOrReleasedMarkCannotBeChangedByLecturer (PASS) |
| ST-17 | UC10 E5 database error | UC10.E5_DatabaseErrorLeavesNothingSaved (PASS) |
| ST-18 | UC14 main | UC14.Main_ApproveThenRelease (PASS) |
| ST-19 | UC14 A1 adjust | UC14.A1_AdjustAMark (PASS) |
| ST-20 | UC14 A2 partial release | UC14.A2_PartialRelease (PASS) |
| ST-21 | UC14 A3 correct released | UC14.A3_CorrectAReleasedMark (PASS) |
| ST-22 | UC14 E1 skipped | UC14.E1_UnmarkedSubmissionIsSkipped (PASS) |
| ST-23 | UC14 E2 other department | UC14.E2_CourseOutsideDepartment (PASS) |
| ST-24 | UC14 E3 reason or range | UC14.E3_ReasonMissingOrMarkOutOfRange (PASS) |
| ST-25 | UC14 E4 batch rollback | UC14.E4_BatchRolledBackOnDatabaseError (PASS) |
| ST-26 | Access control | AccessControl.EachRoleIsLimitedToItsOwnFunctions (PASS) |
| ST-27 | Deactivated user | AccessControl.DeactivatedUserCannotPerformActions (PASS) |

## 6. Acceptance scenarios (validation against the stakeholder statements)

| ID | Stakeholder statement | Demonstrated by | Result |
|---|---|---|---|
| AT-01 | Principal: students must not see any mark until the head of department has checked it | Moderation.StudentsNeverSeeUnreleasedMarks; Console.StudentSeesNothingBeforeRelease | Pass |
| AT-02 | Lecturer: give a student (or a class) more time | Submission.ExtensionMovesTheEffectiveDeadline; CR1.ClassExtensionGivesEveryEnrolledStudentTheNewDeadline | Pass |
| AT-03 | Head of department: 10% per day for up to three days, then zero; see every mark before release | Penalty.BoundaryValuesForDeadlineAndDays; Moderation.ReviewIsLimitedToOwnDepartment; UC14.Main_ApproveThenRelease | Pass |
| AT-04 | Student representative: fix and resubmit before the deadline; written feedback; late work not rejected | UC18.A1_ResubmissionBeforeDeadline; UC10.E2_FeedbackMissing; UC18.A2_LateSubmissionIsAcceptedAndFlagged | Pass |
| AT-05 | IT officer: works offline, passwords protected, fast with 800 students | NFR-03 inspection; Auth.PasswordsAreStoredAsSaltedHashesOnly; Performance test | Pass |
| AT-06 | Principal: simple for teachers who are not IT experts | NFR-08 usability test with three first-time lecturers | **Not yet verified** (needs real users) |

## 7. Regression test for CR1

The 93 tests that existed at v1.0.0 were re-run on v1.1.0 without changing any test case: **93 of 93 passed**. Only the shared test fixture (`tests/fixture.hpp`) and the performance test gained the new constructor argument of `AssignmentService`. The regression run found one real problem during the change (DEF-05). New tests: 10 for CR1 and 1 for NFR-10, giving 104.

## 8. Limits of this testing

* The CI pipeline is defined (`.github/workflows/ci.yml`) but the results above were produced by running the same commands locally; no run on GitHub is claimed.
* Tested on Ubuntu 24.04 only; Windows 10 is supported through WSL2 but was not tested.
* NFR-08 (usability with real lecturers) is not verified.
* Performance was measured on a modern computer, not on the school's old hardware.
