# User manual (version 1.1)

Start the program (your technician will tell you how). Type your **username** and press Enter, then your **password**
(the letters do not show while you type). To leave the program, press Enter with an empty username.

Every menu has numbers. Type the number and press Enter. **0** logs you out. If you type something the program does not
understand, it tells you what to do and you can try again; nothing is lost.

Dates and times are typed as `YYYY-MM-DD HH:MM`, for example `2026-11-20 17:00`.

## All users: changing your password
Choose **Change my password**. Type your current password, then a new one. A new password needs at least 8 characters
with a letter and a digit. After 5 wrong passwords in a row an account is locked for 5 minutes.

## Student
Menu: 1 My courses and assignments, 2 Submit work, 3 See a released mark and feedback, 4 Change my password.

* **See your work to do.** Choose 1. You see each assignment, its maximum mark, its deadline and your status
  (NOT SUBMITTED, SUBMITTED, LATE, MARKED, MODERATED or RELEASED). If your lecturer has given you more time, the line
  also shows **(extended to ...)** with your new deadline.
* **Submit work.** Choose 2, then the course number, then the assignment number, then type where the file is
  (for example `C:\work\essay.pdf` or `/home/sam/essay.pdf`). A file must not be empty and must be 5 MB or smaller.
  The program shows the version number and the time it received your work.
* **Fix a mistake.** Submit again before the deadline. The newest version is the one that is marked; older versions are kept.
* **Late work.** You can still submit after the deadline. The program tells you how many days late you are. Each started
  24 hours costs 10% of the assignment's maximum mark, for up to 3 days; after that the mark is zero. Late work cannot be replaced.
* **See your mark.** Choose 3. A mark and the written feedback appear only after your head of department has released them.

## Lecturer
Menu: 1 My courses and assignments, 2 Create an assignment, 3 Grant an extension to a student, 4 View submissions of an
assignment, 5 Mark a submission, 6 Re-mark a submission, 7 Grant an extension to a whole class, 8 Change my password.

* **Create an assignment.** Choose 2: course, title, description, deadline and maximum mark (1 to 1000). The deadline must be in the future.
* **Grant an extension.** Choose 3: course, assignment, the student, the new deadline (later than the assignment deadline)
  and a reason of at least 5 characters. Late penalties then count from the new deadline for that student.
* **Extend for the whole class (new in version 1.1).** Choose 7: course, assignment, the new deadline (later than the
  assignment deadline) and a reason. Every student enrolled in the course gets the new deadline. A student who already
  has a later extension keeps it; the program tells you how many students were changed. Students who join the course
  later are not included, so repeat the action for them.
* **Mark.** Marking opens after the student's deadline. Choose 4 to see the submission numbers, then 5. Type the mark
  (0 to the maximum, before any late penalty) and written feedback. The program shows the penalty and the final mark.
  The student cannot see the mark yet: the head of department must release it first.
* **Change a mark.** Choose 6 with a reason. This works only until the head of department has approved the mark.
  The old and new values and your reason are recorded.

## Head of department
Menu: 1 Courses and assignments in my department, 2 Review marks of an assignment, 3 Adjust a mark before approval,
4 Approve marks, 5 Release approved marks, 6 Correct a released mark, 7 View the audit log, 8 Change my password.

* **Review.** Choose 2 to see every submission for an assignment with the raw mark, late days and final mark.
* **Adjust.** Choose 3 with the submission number, the new final mark and a reason (recorded).
* **Approve and release.** Choose 4 and type a submission number or `ALL` to approve all marked work. Then choose 5 the same way to release.
  Students see their marks only after release. Work that is not in the right state is skipped and counted.
* **Correct after release.** Choose 6 with a reason. The student sees the corrected mark.
* **Audit log.** Choose 7 to see who changed which mark, when, the old and new value and why (your department only).

## Administrator
Menu: 1 Create a user account, 2 List user accounts, 3 Deactivate or activate an account, 4 Create a department,
5 Create a course, 6 Enrol a student in a course, 7 Assign a lecturer to a course, 8 List departments and courses,
9 View the audit log, 10 Back up the database, 11 Change my password.

* **Create accounts.** Choose 1, pick the role, then username (3 to 32 letters, digits, underscore or full stop), full name and a first password.
  A head of department needs a department; each department has one head.
* **Courses.** Create the department first (4), then the course (5), then enrol students (6) and assign lecturers (7) using the account numbers from option 2.
* **Back up.** Choose 10 and type a file name. Do this every week.
