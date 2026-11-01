#include "data.hpp"

namespace lms {

const char* const kSchemaSql = R"SQL(
CREATE TABLE department (
  id   INTEGER PRIMARY KEY,
  name TEXT NOT NULL UNIQUE
);
CREATE TABLE user_account (
  id            INTEGER PRIMARY KEY,
  username      TEXT NOT NULL UNIQUE COLLATE NOCASE,
  full_name     TEXT NOT NULL,
  role          TEXT NOT NULL CHECK (role IN ('STUDENT','LECTURER','HOD','ADMIN')),
  password_hash TEXT NOT NULL,
  department_id INTEGER REFERENCES department(id),
  is_active     INTEGER NOT NULL DEFAULT 1 CHECK (is_active IN (0,1)),
  failed_logins INTEGER NOT NULL DEFAULT 0 CHECK (failed_logins >= 0),
  locked_until  TEXT,
  CHECK ((role = 'HOD') = (department_id IS NOT NULL))
);
CREATE UNIQUE INDEX ux_hod_department ON user_account(department_id) WHERE role = 'HOD';
CREATE TABLE course (
  id            INTEGER PRIMARY KEY,
  code          TEXT NOT NULL UNIQUE,
  title         TEXT NOT NULL,
  department_id INTEGER NOT NULL REFERENCES department(id)
);
CREATE TABLE enrolment (
  student_id INTEGER NOT NULL REFERENCES user_account(id),
  course_id  INTEGER NOT NULL REFERENCES course(id),
  PRIMARY KEY (student_id, course_id)
);
CREATE TABLE teaching (
  lecturer_id INTEGER NOT NULL REFERENCES user_account(id),
  course_id   INTEGER NOT NULL REFERENCES course(id),
  PRIMARY KEY (lecturer_id, course_id)
);
CREATE TABLE assignment (
  id          INTEGER PRIMARY KEY,
  course_id   INTEGER NOT NULL REFERENCES course(id),
  title       TEXT NOT NULL,
  description TEXT NOT NULL,
  deadline    TEXT NOT NULL,
  max_mark    INTEGER NOT NULL CHECK (max_mark BETWEEN 1 AND 1000),
  created_by  INTEGER NOT NULL REFERENCES user_account(id),
  created_at  TEXT NOT NULL
);
CREATE TABLE extension (
  id            INTEGER PRIMARY KEY,
  assignment_id INTEGER NOT NULL REFERENCES assignment(id),
  student_id    INTEGER NOT NULL REFERENCES user_account(id),
  new_deadline  TEXT NOT NULL,
  reason        TEXT NOT NULL CHECK (length(reason) >= 5),
  granted_by    INTEGER NOT NULL REFERENCES user_account(id),
  granted_at    TEXT NOT NULL,
  UNIQUE (assignment_id, student_id)
);
CREATE TABLE submission (
  id            INTEGER PRIMARY KEY,
  assignment_id INTEGER NOT NULL REFERENCES assignment(id),
  student_id    INTEGER NOT NULL REFERENCES user_account(id),
  state         TEXT NOT NULL CHECK (state IN ('SUBMITTED','LATE','MARKED','MODERATED','RELEASED')),
  UNIQUE (assignment_id, student_id)
);
CREATE TABLE submission_version (
  id            INTEGER PRIMARY KEY,
  submission_id INTEGER NOT NULL REFERENCES submission(id) ON DELETE CASCADE,
  version_no    INTEGER NOT NULL CHECK (version_no >= 1),
  stored_path   TEXT NOT NULL,
  original_name TEXT NOT NULL,
  size_bytes    INTEGER NOT NULL CHECK (size_bytes BETWEEN 1 AND 5242880),
  submitted_at  TEXT NOT NULL,
  UNIQUE (submission_id, version_no)
);
CREATE TABLE mark (
  submission_id INTEGER PRIMARY KEY REFERENCES submission(id),
  raw_mark      REAL NOT NULL CHECK (raw_mark >= 0),
  days_late     INTEGER NOT NULL CHECK (days_late >= 0),
  final_mark    REAL NOT NULL CHECK (final_mark >= 0),
  feedback      TEXT NOT NULL CHECK (length(trim(feedback)) > 0),
  marked_by     INTEGER NOT NULL REFERENCES user_account(id),
  marked_at     TEXT NOT NULL,
  approved_by   INTEGER REFERENCES user_account(id),
  approved_at   TEXT,
  released_by   INTEGER REFERENCES user_account(id),
  released_at   TEXT,
  CHECK ((approved_by IS NULL) = (approved_at IS NULL)),
  CHECK ((released_by IS NULL) = (released_at IS NULL)),
  CHECK (released_at IS NULL OR approved_at IS NOT NULL)
);
CREATE TABLE audit_log (
  id            INTEGER PRIMARY KEY AUTOINCREMENT,
  submission_id INTEGER NOT NULL REFERENCES submission(id),
  changed_by    INTEGER NOT NULL REFERENCES user_account(id),
  changed_at    TEXT NOT NULL,
  action        TEXT NOT NULL CHECK (action IN ('INITIAL_MARK','REMARK','MODERATION_ADJUST','RELEASED_CORRECTION')),
  old_value     REAL,
  new_value     REAL NOT NULL,
  reason        TEXT NOT NULL CHECK (length(reason) >= 5)
);
CREATE TRIGGER audit_no_update BEFORE UPDATE ON audit_log
  BEGIN SELECT RAISE(ABORT, 'audit log is append-only'); END;
CREATE TRIGGER audit_no_delete BEFORE DELETE ON audit_log
  BEGIN SELECT RAISE(ABORT, 'audit log is append-only'); END;
CREATE INDEX ix_enrolment_course   ON enrolment(course_id);
CREATE INDEX ix_teaching_course    ON teaching(course_id);
CREATE INDEX ix_assignment_course  ON assignment(course_id);
CREATE INDEX ix_submission_assign  ON submission(assignment_id);
CREATE INDEX ix_submission_student ON submission(student_id);
CREATE INDEX ix_audit_submission   ON audit_log(submission_id);
)SQL";

}  // namespace lms
