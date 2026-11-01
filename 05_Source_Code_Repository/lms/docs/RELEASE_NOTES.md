# Release notes

## v1.1.0 (change request CR1)

New
* A lecturer can extend an assignment's deadline for a whole class in one step (lecturer menu item 7). Students who
  already have a later extension keep it. The change is all-or-nothing.
* Students see their extended deadline on their assignment list, for example `(extended to 2026-11-06 17:00)`.

Changed
* The lecturer menu has a new item 7; "Change my password" is now item 8. All other menu numbers are unchanged.
* The program version shown by `lms --version` is 1.1.0.

Upgrading from 1.0.0
* Take a backup first. Replace the `lms` program. The database does not change, so no conversion is needed.

Known limitations
* Students who join a course after a class extension was granted are not included; repeat the action for them.

## v1.0.0 (first release)

Features
* Accounts for four roles (student, lecturer, head of department, administrator) with role-based menus.
* Administrator: accounts, departments, courses, enrolments, lecturer assignments, audit log, database backup.
* Lecturer: assignments with deadline and maximum mark, individual extensions, marking with written feedback, re-marking with a reason.
* Student: view assignments, submit, resubmit before the deadline (all versions kept), late submission accepted and flagged.
* Late penalty: 10% of the maximum mark for each started 24 hours, up to 3 days, then zero.
* Head of department: review, adjust, approve and release marks; correct released marks. Students see marks only after release.
* Every mark change is recorded (who, when, old value, new value, reason) in an append-only audit log.
* Passwords stored as salted Argon2id hashes; account lock after 5 wrong passwords; works fully offline.

Known limitations
* Console interface only (no graphical or web interface).
* Extensions are given one student at a time. (Whole-class extensions are requested in CR1 for v1.1.)
* Up to 5 people at the same time on one database file.
