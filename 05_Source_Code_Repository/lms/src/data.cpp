#include "data.hpp"

#include <sqlite3.h>

namespace lms {

// ---------------- Statement ----------------
namespace {
[[noreturn]] void fail(sqlite3* db, int rc) {
  const std::string msg = db ? sqlite3_errmsg(db) : "unknown error";
  if (rc == SQLITE_CONSTRAINT) throw DatabaseError("That change was refused because it breaks a data rule: " + msg);
  throw DatabaseError("The database reported a problem: " + msg);
}
}  // namespace

Statement::Statement(sqlite3* db, const std::string& sql) : db_(db), st_(nullptr) {
  const int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &st_, nullptr);
  if (rc != SQLITE_OK) fail(db_, rc);
}
Statement::~Statement() { if (st_) sqlite3_finalize(st_); }
Statement::Statement(Statement&& o) noexcept : db_(o.db_), st_(o.st_) { o.st_ = nullptr; }

Statement& Statement::bind(int i, std::int64_t v) {
  if (int rc = sqlite3_bind_int64(st_, i, v); rc != SQLITE_OK) fail(db_, rc);
  return *this;
}
Statement& Statement::bind(int i, double v) {
  if (int rc = sqlite3_bind_double(st_, i, v); rc != SQLITE_OK) fail(db_, rc);
  return *this;
}
Statement& Statement::bind(int i, const std::string& v) {
  if (int rc = sqlite3_bind_text(st_, i, v.c_str(), static_cast<int>(v.size()), SQLITE_TRANSIENT); rc != SQLITE_OK) fail(db_, rc);
  return *this;
}
Statement& Statement::bindNull(int i) {
  if (int rc = sqlite3_bind_null(st_, i); rc != SQLITE_OK) fail(db_, rc);
  return *this;
}
Statement& Statement::bind(int i, const std::optional<int>& v) { return v ? bind(i, *v) : bindNull(i); }
Statement& Statement::bind(int i, const std::optional<TimePoint>& v) { return v ? bind(i, toIso(*v)) : bindNull(i); }
Statement& Statement::bind(int i, const std::optional<double>& v) { return v ? bind(i, *v) : bindNull(i); }

bool Statement::step() {
  const int rc = sqlite3_step(st_);
  if (rc == SQLITE_ROW) return true;
  if (rc == SQLITE_DONE) return false;
  fail(db_, rc);
}
void Statement::run() { step(); }
std::int64_t Statement::integer(int c) const { return sqlite3_column_int64(st_, c); }
double Statement::real(int c) const { return sqlite3_column_double(st_, c); }
std::string Statement::text(int c) const {
  const unsigned char* t = sqlite3_column_text(st_, c);
  return t ? reinterpret_cast<const char*>(t) : "";
}
bool Statement::isNull(int c) const { return sqlite3_column_type(st_, c) == SQLITE_NULL; }

// ---------------- Database ----------------
Database::Database(const std::string& path) {
  const int rc = sqlite3_open_v2(path.c_str(), &db_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
  if (rc != SQLITE_OK) {
    const std::string msg = db_ ? sqlite3_errmsg(db_) : "out of memory";
    sqlite3_close(db_);
    db_ = nullptr;
    throw DatabaseError("The database file could not be opened: " + msg);
  }
  sqlite3_busy_timeout(db_, 5000);
  exec("PRAGMA foreign_keys = ON;");
  Statement probe = prepare("SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = 'user_account'");
  if (!probe.step()) exec(kSchemaSql);
}
Database::~Database() { if (db_) sqlite3_close(db_); }
Statement Database::prepare(const std::string& sql) { return Statement(db_, sql); }
void Database::exec(const std::string& sql) {
  char* err = nullptr;
  const int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err);
  if (rc != SQLITE_OK) {
    const std::string msg = err ? err : "unknown error";
    sqlite3_free(err);
    throw DatabaseError("The database reported a problem: " + msg);
  }
}
std::int64_t Database::lastInsertId() const { return sqlite3_last_insert_rowid(db_); }

void Database::backupTo(const std::string& path) {
  sqlite3* dst = nullptr;
  if (sqlite3_open(path.c_str(), &dst) != SQLITE_OK) {
    sqlite3_close(dst);
    throw DatabaseError("The backup file could not be created. Check the folder name and permissions.");
  }
  sqlite3_backup* b = sqlite3_backup_init(dst, "main", db_, "main");
  int rc = SQLITE_ERROR;
  if (b) {
    sqlite3_backup_step(b, -1);
    rc = sqlite3_backup_finish(b);
  }
  sqlite3_close(dst);
  if (rc != SQLITE_OK) throw DatabaseError("The backup could not be completed.");
}

// ---------------- Unit of work ----------------
class ScopedTx : public IScopedTransaction {
 public:
  explicit ScopedTx(SqliteUnitOfWork& u) : u_(u), outer_(u.depth_ == 0) {
    if (outer_) { u_.db_.exec("BEGIN IMMEDIATE"); u_.failed_ = false; }
    u_.depth_++;
  }
  ~ScopedTx() override {
    u_.depth_--;
    if (!done_) {
      u_.failed_ = true;
      if (outer_) { try { u_.db_.exec("ROLLBACK"); } catch (...) {} }
    }
  }
  void commit() override {
    if (done_) return;
    done_ = true;
    if (!outer_) return;
    if (u_.failed_) {
      u_.db_.exec("ROLLBACK");
      throw DatabaseError("The change was cancelled because a related step failed.");
    }
    u_.db_.exec("COMMIT");
  }
 private:
  SqliteUnitOfWork& u_;
  bool outer_;
  bool done_ = false;
};
std::unique_ptr<IScopedTransaction> SqliteUnitOfWork::begin() { return std::make_unique<ScopedTx>(*this); }

// ---------------- Row readers ----------------
namespace {
std::optional<int> optInt(const Statement& s, int c) {
  if (s.isNull(c)) return std::nullopt;
  return static_cast<int>(s.integer(c));
}
std::optional<TimePoint> optTime(const Statement& s, int c) {
  if (s.isNull(c)) return std::nullopt;
  return fromIso(s.text(c));
}
std::unique_ptr<User> readUser(const Statement& s) {
  NewUser d{s.text(1), s.text(2), s.text(4), optInt(s, 5)};
  return UserFactory::create(roleFromString(s.text(3)), static_cast<int>(s.integer(0)), d, s.integer(6) != 0);
}
const char* const kUserCols = "id, username, full_name, role, password_hash, department_id, is_active";
Course readCourse(const Statement& s) {
  return {static_cast<int>(s.integer(0)), s.text(1), s.text(2), static_cast<int>(s.integer(3))};
}
const char* const kCourseCols = "c.id, c.code, c.title, c.department_id";
Assignment readAssignment(const Statement& s) {
  Assignment a;
  a.id = static_cast<int>(s.integer(0)); a.courseId = static_cast<int>(s.integer(1));
  a.title = s.text(2); a.description = s.text(3); a.deadline = fromIso(s.text(4));
  a.maxMark = static_cast<int>(s.integer(5)); a.createdBy = static_cast<int>(s.integer(6));
  a.createdAt = fromIso(s.text(7));
  return a;
}
Mark readMark(const Statement& s, int b) {  // columns b..b+10 from the mark table
  Mark m;
  m.submissionId = static_cast<int>(s.integer(b));
  m.rawMark = s.real(b + 1); m.daysLate = static_cast<int>(s.integer(b + 2)); m.finalMark = s.real(b + 3);
  m.feedback = s.text(b + 4); m.markedBy = static_cast<int>(s.integer(b + 5)); m.markedAt = fromIso(s.text(b + 6));
  m.approvedBy = optInt(s, b + 7); m.approvedAt = optTime(s, b + 8);
  m.releasedBy = optInt(s, b + 9); m.releasedAt = optTime(s, b + 10);
  return m;
}
}  // namespace

// ---------------- Users ----------------
std::unique_ptr<User> SqliteUserRepository::findById(int id) {
  Statement st = db_.prepare(std::string("SELECT ") + kUserCols + " FROM user_account WHERE id = ?");
  st.bind(1, id);
  return st.step() ? readUser(st) : nullptr;
}
std::unique_ptr<User> SqliteUserRepository::findByUsername(const std::string& username) {
  Statement st = db_.prepare(std::string("SELECT ") + kUserCols + " FROM user_account WHERE username = ?");
  st.bind(1, username);
  return st.step() ? readUser(st) : nullptr;
}
int SqliteUserRepository::add(Role role, const NewUser& d) {
  Statement st = db_.prepare(
      "INSERT INTO user_account (username, full_name, role, password_hash, department_id) VALUES (?, ?, ?, ?, ?)");
  st.bind(1, d.username).bind(2, d.fullName).bind(3, toString(role)).bind(4, d.passwordHash).bind(5, d.departmentId);
  st.run();
  return static_cast<int>(db_.lastInsertId());
}
void SqliteUserRepository::setActive(int id, bool active) {
  Statement st = db_.prepare("UPDATE user_account SET is_active = ? WHERE id = ?");
  st.bind(1, active ? 1 : 0).bind(2, id);
  st.run();
}
void SqliteUserRepository::setPasswordHash(int id, const std::string& hash) {
  Statement st = db_.prepare("UPDATE user_account SET password_hash = ? WHERE id = ?");
  st.bind(1, hash).bind(2, id);
  st.run();
}
LoginState SqliteUserRepository::loginState(int id) {
  Statement st = db_.prepare("SELECT failed_logins, locked_until FROM user_account WHERE id = ?");
  st.bind(1, id);
  LoginState s;
  if (st.step()) { s.failedLogins = static_cast<int>(st.integer(0)); s.lockedUntil = optTime(st, 1); }
  return s;
}
void SqliteUserRepository::setLoginState(int id, const LoginState& s) {
  Statement st = db_.prepare("UPDATE user_account SET failed_logins = ?, locked_until = ? WHERE id = ?");
  st.bind(1, s.failedLogins).bind(2, s.lockedUntil).bind(3, id);
  st.run();
}
std::vector<std::unique_ptr<User>> SqliteUserRepository::listAll() {
  Statement st = db_.prepare(std::string("SELECT ") + kUserCols + " FROM user_account ORDER BY role, full_name");
  std::vector<std::unique_ptr<User>> out;
  while (st.step()) out.push_back(readUser(st));
  return out;
}

// ---------------- Courses ----------------
int SqliteCourseRepository::addDepartment(const std::string& name) {
  Statement st = db_.prepare("INSERT INTO department (name) VALUES (?)");
  st.bind(1, name);
  st.run();
  return static_cast<int>(db_.lastInsertId());
}
std::vector<Department> SqliteCourseRepository::listDepartments() {
  Statement st = db_.prepare("SELECT id, name FROM department ORDER BY name");
  std::vector<Department> out;
  while (st.step()) out.push_back({static_cast<int>(st.integer(0)), st.text(1)});
  return out;
}
std::optional<Department> SqliteCourseRepository::findDepartment(int id) {
  Statement st = db_.prepare("SELECT id, name FROM department WHERE id = ?");
  st.bind(1, id);
  if (!st.step()) return std::nullopt;
  return Department{static_cast<int>(st.integer(0)), st.text(1)};
}
int SqliteCourseRepository::addCourse(const Course& c) {
  Statement st = db_.prepare("INSERT INTO course (code, title, department_id) VALUES (?, ?, ?)");
  st.bind(1, c.code).bind(2, c.title).bind(3, c.departmentId);
  st.run();
  return static_cast<int>(db_.lastInsertId());
}
std::vector<Course> SqliteCourseRepository::query(const std::string& sql, int arg, bool hasArg) {
  Statement st = db_.prepare(sql);
  if (hasArg) st.bind(1, arg);
  std::vector<Course> out;
  while (st.step()) out.push_back(readCourse(st));
  return out;
}
std::optional<Course> SqliteCourseRepository::findCourse(int id) {
  auto v = query(std::string("SELECT ") + kCourseCols + " FROM course c WHERE c.id = ?", id, true);
  if (v.empty()) return std::nullopt;
  return v.front();
}
std::vector<Course> SqliteCourseRepository::listCourses() {
  return query(std::string("SELECT ") + kCourseCols + " FROM course c ORDER BY c.code", 0, false);
}
std::vector<Course> SqliteCourseRepository::coursesForDepartment(int departmentId) {
  return query(std::string("SELECT ") + kCourseCols + " FROM course c WHERE c.department_id = ? ORDER BY c.code", departmentId, true);
}
std::vector<Course> SqliteCourseRepository::coursesForStudent(int studentId) {
  return query(std::string("SELECT ") + kCourseCols +
               " FROM course c JOIN enrolment e ON e.course_id = c.id WHERE e.student_id = ? ORDER BY c.code", studentId, true);
}
std::vector<Course> SqliteCourseRepository::coursesForLecturer(int lecturerId) {
  return query(std::string("SELECT ") + kCourseCols +
               " FROM course c JOIN teaching t ON t.course_id = c.id WHERE t.lecturer_id = ? ORDER BY c.code", lecturerId, true);
}
void SqliteCourseRepository::enrol(int studentId, int courseId) {
  Statement st = db_.prepare("INSERT INTO enrolment (student_id, course_id) VALUES (?, ?)");
  st.bind(1, studentId).bind(2, courseId);
  st.run();
}
void SqliteCourseRepository::assignLecturer(int lecturerId, int courseId) {
  Statement st = db_.prepare("INSERT INTO teaching (lecturer_id, course_id) VALUES (?, ?)");
  st.bind(1, lecturerId).bind(2, courseId);
  st.run();
}
bool SqliteCourseRepository::isEnrolled(int studentId, int courseId) {
  Statement st = db_.prepare("SELECT 1 FROM enrolment WHERE student_id = ? AND course_id = ?");
  st.bind(1, studentId).bind(2, courseId);
  return st.step();
}
bool SqliteCourseRepository::teaches(int lecturerId, int courseId) {
  Statement st = db_.prepare("SELECT 1 FROM teaching WHERE lecturer_id = ? AND course_id = ?");
  st.bind(1, lecturerId).bind(2, courseId);
  return st.step();
}
std::vector<int> SqliteCourseRepository::studentsInCourse(int courseId) {
  Statement st = db_.prepare("SELECT student_id FROM enrolment WHERE course_id = ? ORDER BY student_id");
  st.bind(1, courseId);
  std::vector<int> out;
  while (st.step()) out.push_back(static_cast<int>(st.integer(0)));
  return out;
}

// ---------------- Assignments and extensions ----------------
int SqliteAssignmentRepository::add(const Assignment& a) {
  Statement st = db_.prepare(
      "INSERT INTO assignment (course_id, title, description, deadline, max_mark, created_by, created_at) "
      "VALUES (?, ?, ?, ?, ?, ?, ?)");
  st.bind(1, a.courseId).bind(2, a.title).bind(3, a.description).bind(4, toIso(a.deadline))
      .bind(5, a.maxMark).bind(6, a.createdBy).bind(7, toIso(a.createdAt));
  st.run();
  return static_cast<int>(db_.lastInsertId());
}
std::optional<Assignment> SqliteAssignmentRepository::findById(int id) {
  Statement st = db_.prepare(
      "SELECT id, course_id, title, description, deadline, max_mark, created_by, created_at FROM assignment WHERE id = ?");
  st.bind(1, id);
  if (!st.step()) return std::nullopt;
  return readAssignment(st);
}
std::vector<Assignment> SqliteAssignmentRepository::listByCourse(int courseId) {
  Statement st = db_.prepare(
      "SELECT id, course_id, title, description, deadline, max_mark, created_by, created_at "
      "FROM assignment WHERE course_id = ? ORDER BY deadline, id");
  st.bind(1, courseId);
  std::vector<Assignment> out;
  while (st.step()) out.push_back(readAssignment(st));
  return out;
}
int SqliteAssignmentRepository::saveExtension(const Extension& e) {
  Statement st = db_.prepare(
      "INSERT INTO extension (assignment_id, student_id, new_deadline, reason, granted_by, granted_at) "
      "VALUES (?, ?, ?, ?, ?, ?) ON CONFLICT(assignment_id, student_id) DO UPDATE SET "
      "new_deadline = excluded.new_deadline, reason = excluded.reason, "
      "granted_by = excluded.granted_by, granted_at = excluded.granted_at");
  st.bind(1, e.assignmentId).bind(2, e.studentId).bind(3, toIso(e.newDeadline)).bind(4, e.reason)
      .bind(5, e.grantedBy).bind(6, toIso(e.grantedAt));
  st.run();
  Statement q = db_.prepare("SELECT id FROM extension WHERE assignment_id = ? AND student_id = ?");
  q.bind(1, e.assignmentId).bind(2, e.studentId);
  q.step();
  return static_cast<int>(q.integer(0));
}
std::optional<Extension> SqliteAssignmentRepository::findExtension(int assignmentId, int studentId) {
  Statement st = db_.prepare(
      "SELECT id, assignment_id, student_id, new_deadline, reason, granted_by, granted_at FROM extension "
      "WHERE assignment_id = ? AND student_id = ?");
  st.bind(1, assignmentId).bind(2, studentId);
  if (!st.step()) return std::nullopt;
  Extension e;
  e.id = static_cast<int>(st.integer(0)); e.assignmentId = static_cast<int>(st.integer(1));
  e.studentId = static_cast<int>(st.integer(2)); e.newDeadline = fromIso(st.text(3)); e.reason = st.text(4);
  e.grantedBy = static_cast<int>(st.integer(5)); e.grantedAt = fromIso(st.text(6));
  return e;
}

// ---------------- Submissions ----------------
std::optional<Submission> SqliteSubmissionRepository::load(Statement& st) {
  if (!st.step()) return std::nullopt;
  Submission s;
  s.id = static_cast<int>(st.integer(0)); s.assignmentId = static_cast<int>(st.integer(1));
  s.studentId = static_cast<int>(st.integer(2)); s.state = stateFromString(st.text(3));
  Statement v = db_.prepare(
      "SELECT id, version_no, stored_path, original_name, size_bytes, submitted_at FROM submission_version "
      "WHERE submission_id = ? ORDER BY version_no");
  v.bind(1, s.id);
  while (v.step()) {
    SubmissionVersion x;
    x.id = static_cast<int>(v.integer(0)); x.versionNo = static_cast<int>(v.integer(1));
    x.storedPath = v.text(2); x.originalName = v.text(3); x.sizeBytes = v.integer(4);
    x.submittedAt = fromIso(v.text(5));
    s.versions.push_back(x);
  }
  return s;
}
std::optional<Submission> SqliteSubmissionRepository::findById(int id) {
  Statement st = db_.prepare("SELECT id, assignment_id, student_id, state FROM submission WHERE id = ?");
  st.bind(1, id);
  return load(st);
}
std::optional<Submission> SqliteSubmissionRepository::findByStudentAndAssignment(int studentId, int assignmentId) {
  Statement st = db_.prepare(
      "SELECT id, assignment_id, student_id, state FROM submission WHERE student_id = ? AND assignment_id = ?");
  st.bind(1, studentId).bind(2, assignmentId);
  return load(st);
}
int SqliteSubmissionRepository::add(const Submission& s) {
  Statement st = db_.prepare("INSERT INTO submission (assignment_id, student_id, state) VALUES (?, ?, ?)");
  st.bind(1, s.assignmentId).bind(2, s.studentId).bind(3, toString(s.state));
  st.run();
  const int id = static_cast<int>(db_.lastInsertId());
  for (const auto& v : s.versions) addVersion(id, v);
  return id;
}
int SqliteSubmissionRepository::addVersion(int submissionId, const SubmissionVersion& v) {
  Statement st = db_.prepare(
      "INSERT INTO submission_version (submission_id, version_no, stored_path, original_name, size_bytes, submitted_at) "
      "VALUES (?, ?, ?, ?, ?, ?)");
  st.bind(1, submissionId).bind(2, v.versionNo).bind(3, v.storedPath).bind(4, v.originalName)
      .bind(5, v.sizeBytes).bind(6, toIso(v.submittedAt));
  st.run();
  return static_cast<int>(db_.lastInsertId());
}
void SqliteSubmissionRepository::setState(int id, SubmissionState s) {
  Statement st = db_.prepare("UPDATE submission SET state = ? WHERE id = ?");
  st.bind(1, toString(s)).bind(2, id);
  st.run();
}

// ---------------- Marks ----------------
std::optional<Mark> SqliteMarkRepository::find(int submissionId) {
  Statement st = db_.prepare(
      "SELECT submission_id, raw_mark, days_late, final_mark, feedback, marked_by, marked_at, approved_by, "
      "approved_at, released_by, released_at FROM mark WHERE submission_id = ?");
  st.bind(1, submissionId);
  if (!st.step()) return std::nullopt;
  return readMark(st, 0);
}
void SqliteMarkRepository::upsert(const Mark& m) {
  Statement st = db_.prepare(
      "INSERT INTO mark (submission_id, raw_mark, days_late, final_mark, feedback, marked_by, marked_at) "
      "VALUES (?, ?, ?, ?, ?, ?, ?) ON CONFLICT(submission_id) DO UPDATE SET raw_mark = excluded.raw_mark, "
      "days_late = excluded.days_late, final_mark = excluded.final_mark, feedback = excluded.feedback, "
      "marked_by = excluded.marked_by, marked_at = excluded.marked_at");
  st.bind(1, m.submissionId).bind(2, m.rawMark).bind(3, m.daysLate).bind(4, m.finalMark).bind(5, m.feedback)
      .bind(6, m.markedBy).bind(7, toIso(m.markedAt));
  st.run();
}
void SqliteMarkRepository::setApproval(int submissionId, int by, TimePoint at) {
  Statement st = db_.prepare("UPDATE mark SET approved_by = ?, approved_at = ? WHERE submission_id = ?");
  st.bind(1, by).bind(2, toIso(at)).bind(3, submissionId);
  st.run();
}
void SqliteMarkRepository::setRelease(int submissionId, int by, TimePoint at) {
  Statement st = db_.prepare("UPDATE mark SET released_by = ?, released_at = ? WHERE submission_id = ?");
  st.bind(1, by).bind(2, toIso(at)).bind(3, submissionId);
  st.run();
}
void SqliteMarkRepository::setFinalMark(int submissionId, double finalMark) {
  Statement st = db_.prepare("UPDATE mark SET final_mark = ? WHERE submission_id = ?");
  st.bind(1, finalMark).bind(2, submissionId);
  st.run();
}
std::vector<SubmissionRow> SqliteMarkRepository::rowsByAssignment(int assignmentId) {
  Statement st = db_.prepare(
      "SELECT s.id, s.assignment_id, s.student_id, s.state, u.full_name, "
      "(SELECT MAX(v.submitted_at) FROM submission_version v WHERE v.submission_id = s.id), "
      "(SELECT COUNT(*) FROM submission_version v WHERE v.submission_id = s.id), "
      "m.submission_id, m.raw_mark, m.days_late, m.final_mark, m.feedback, m.marked_by, m.marked_at, "
      "m.approved_by, m.approved_at, m.released_by, m.released_at "
      "FROM submission s JOIN user_account u ON u.id = s.student_id "
      "LEFT JOIN mark m ON m.submission_id = s.id WHERE s.assignment_id = ? ORDER BY u.full_name, s.id");
  st.bind(1, assignmentId);
  std::vector<SubmissionRow> out;
  while (st.step()) {
    SubmissionRow r;
    r.submission.id = static_cast<int>(st.integer(0));
    r.submission.assignmentId = static_cast<int>(st.integer(1));
    r.submission.studentId = static_cast<int>(st.integer(2));
    r.submission.state = stateFromString(st.text(3));
    r.studentName = st.text(4);
    r.submittedAt = fromIso(st.text(5));
    r.versionCount = static_cast<int>(st.integer(6));
    if (!st.isNull(7)) r.mark = readMark(st, 7);
    out.push_back(std::move(r));
  }
  return out;
}

// ---------------- Audit log ----------------
int SqliteAuditRepository::add(const AuditEntry& e) {
  Statement st = db_.prepare(
      "INSERT INTO audit_log (submission_id, changed_by, changed_at, action, old_value, new_value, reason) "
      "VALUES (?, ?, ?, ?, ?, ?, ?)");
  st.bind(1, e.submissionId).bind(2, e.changedBy).bind(3, toIso(e.changedAt)).bind(4, toString(e.action))
      .bind(5, e.oldValue).bind(6, e.newValue).bind(7, e.reason);
  st.run();
  return static_cast<int>(db_.lastInsertId());
}
std::vector<AuditEntry> SqliteAuditRepository::list(const AuditFilter& f) {
  std::string sql =
      "SELECT a.id, a.submission_id, a.changed_by, a.changed_at, a.action, a.old_value, a.new_value, a.reason "
      "FROM audit_log a JOIN submission s ON s.id = a.submission_id JOIN assignment g ON g.id = s.assignment_id "
      "JOIN course c ON c.id = g.course_id WHERE 1 = 1";
  if (f.submissionId) sql += " AND a.submission_id = ?";
  if (f.changedBy) sql += " AND a.changed_by = ?";
  if (f.departmentId) sql += " AND c.department_id = ?";
  if (f.from) sql += " AND a.changed_at >= ?";
  if (f.to) sql += " AND a.changed_at <= ?";
  sql += " ORDER BY a.id";
  Statement st = db_.prepare(sql);
  int i = 1;
  if (f.submissionId) st.bind(i++, *f.submissionId);
  if (f.changedBy) st.bind(i++, *f.changedBy);
  if (f.departmentId) st.bind(i++, *f.departmentId);
  if (f.from) st.bind(i++, toIso(*f.from));
  if (f.to) st.bind(i++, toIso(*f.to));
  std::vector<AuditEntry> out;
  while (st.step()) {
    AuditEntry e;
    e.id = static_cast<int>(st.integer(0)); e.submissionId = static_cast<int>(st.integer(1));
    e.changedBy = static_cast<int>(st.integer(2)); e.changedAt = fromIso(st.text(3));
    e.action = auditActionFromString(st.text(4));
    if (!st.isNull(5)) e.oldValue = st.real(5);
    e.newValue = st.real(6); e.reason = st.text(7);
    out.push_back(e);
  }
  return out;
}

}  // namespace lms
