# Change request CR1: analysis and implementation record

**Request (from the school):** "A lecturer should be able to extend the deadline for a whole class, not just an individual
student, and extensions should be shown on the student's assignment list."

## 1. Classification
**Perfective maintenance** (ISO/IEC 14764): the system works as specified; the users want new and improved functionality.
It is not *corrective* (there is no fault), not *adaptive* (the environment, data format and platform are unchanged) and not
*preventive* (it does not restructure code to avoid future faults).

## 2. Impact analysis

| Kind | Item | Effect | Reason |
|---|---|---|---|
| Requirement | FR-10 Grant extension | Modified | Applies to one student or, new, to a whole class |
| Requirement | FR-24 (new) | Added | "A lecturer shall be able to extend the deadline of an assignment for every student enrolled in the course in one action; a student who already has a later extension keeps it." (Must) |
| Requirement | FR-06 View assignments | Modified | The list shows the student's extended deadline |
| Requirement | BR-1, BR-10 | Unchanged, apply | The effective deadline rule and the reason rule cover class extensions |
| Requirement | NFR-02, NFR-08 | Re-verified | Class extension for 800 students stays under 1 second; menu text stays plain |
| Use case | UC08 Grant extension | Modified | New alternative flow A1 "whole class" and exception E1 "no students enrolled" |
| Use case | UC17 View courses and assignments | Modified | Step "show assignments" displays the extension |
| Use case | UC22 Grant class extension (new) | Added | Extends UC08 in the use case diagram |
| Use case | UC18, UC10 | Unchanged | They already use the effective deadline (BR-1) |
| Diagram | Use case diagram, class diagram, component diagram | Updated | New use case; new operation and result type; AssignmentService now needs the unit of work |
| Diagram | Sequence diagram "Grant class extension" | Added | Shows the single transaction over all students |
| Diagram | State machine, activity diagram, ERD | Unchanged | No change to the life cycle, process or tables |
| Class | AssignmentService | Modified | New `grantClassExtension()`; new constructor dependency `IUnitOfWork`; `assignmentsFor()` fills `extendedTo` |
| Class | AssignmentRow | Modified | New field `extendedTo` |
| Class | ClassExtensionResult | Added | Counts students granted and students who kept a later extension |
| Class | ConsoleApp | Modified | Lecturer menu item 7; student list text |
| Class | Extension, repositories, MarkingService, SubmissionService, ILatePenaltyPolicy | Unchanged | Existing `findExtension`, `saveExtension` and `studentsInCourse` are reused |
| Table | `extension` | Rows only | One row per student and assignment already exists; no schema change, no data migration |
| Test | Existing 93 tests | Re-run unchanged | Regression suite (only the test fixture wiring gained the new constructor argument) |
| Test | 10 new tests (`CR1.*`) | Added | Rules, validation, atomicity, display, and console flow |
| Document | SRS, SDD, user manual, release notes, traceability matrix, risk register | Updated | See the final report appendices |

## 3. Design decision
A class extension is stored as one `extension` row per enrolled student. Alternatives considered:

* *A separate `class_extension` table:* would also cover students who enrol later, but needs a schema change and a
  migration, and the effective-deadline rule (BR-1) would have to look in two places.
* *One row per student (chosen):* no schema change, BR-1 and the penalty code stay as they are, individual and class
  extensions combine naturally (the later date wins). Cost: students who enrol later are not included.

## 4. Implementation record (v1.1.0)
* Branch `feature/cr1-class-extension`, merged by pull request, tagged `v1.1.0`.
* One regression was found by the existing suite during the change (see the defect log, DEF-05): the first version put
  the new menu item in position 4 and renumbered other options. It was moved to position 7 so existing numbers stay stable.

## 5. Regression and new tests
See `docs/TEST_REPORT.md`: all 93 existing tests pass on v1.1.0 and the 10 new tests pass (103 in total).
