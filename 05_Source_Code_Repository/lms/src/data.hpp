// Data access layer: SQLite wrapper, unit of work and the Repository pattern.
#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "domain.hpp"

struct sqlite3;
struct sqlite3_stmt;

namespace lms {

// ---- SQLite wrapper (all statements are prepared and bound, never concatenated) ----
class Statement {
 public:
  Statement(sqlite3* db, const std::string& sql);
  ~Statement();
  Statement(Statement&& o) noexcept;
  Statement(const Statement&) = delete;
  Statement& operator=(const Statement&) = delete;
  Statement& bind(int i, std::int64_t v);
  Statement& bind(int i, int v) { return bind(i, static_cast<std::int64_t>(v)); }
  Statement& bind(int i, double v);
  Statement& bind(int i, const std::string& v);
  Statement& bindNull(int i);
  Statement& bind(int i, const std::optional<int>& v);
  Statement& bind(int i, const std::optional<TimePoint>& v);  // stored as ISO text
  Statement& bind(int i, const std::optional<double>& v);
  bool step();  // true when a row is available
  void run();   // execute a statement that returns no rows
  std::int64_t integer(int c) const;
  double real(int c) const;
  std::string text(int c) const;
  bool isNull(int c) const;
 private:
  sqlite3* db_;
  sqlite3_stmt* st_;
};

class Database {
 public:
  explicit Database(const std::string& path);  // ":memory:" for tests
  ~Database();
  Database(const Database&) = delete;
  Database& operator=(const Database&) = delete;
  Statement prepare(const std::string& sql);
  void exec(const std::string& sql);
  std::int64_t lastInsertId() const;
  void backupTo(const std::string& path);
  sqlite3* handle() { return db_; }
 private:
  sqlite3* db_ = nullptr;
};

extern const char* const kSchemaSql;

class IScopedTransaction {
 public:
  virtual ~IScopedTransaction() = default;
  virtual void commit() = 0;
};
class IUnitOfWork {
 public:
  virtual ~IUnitOfWork() = default;
  virtual std::unique_ptr<IScopedTransaction> begin() = 0;  // rolls back unless commit() is called
};
class SqliteUnitOfWork : public IUnitOfWork {
 public:
  explicit SqliteUnitOfWork(Database& db) : db_(db) {}
  std::unique_ptr<IScopedTransaction> begin() override;
 private:
  friend class ScopedTx;
  Database& db_;
  int depth_ = 0;
  bool failed_ = false;
};

// ---- Read models ----
struct LoginState { int failedLogins = 0; std::optional<TimePoint> lockedUntil; };
struct SubmissionRow {
  Submission submission;  // versions are not loaded in rows
  std::string studentName;
  std::optional<Mark> mark;
  TimePoint submittedAt = 0;
  int versionCount = 0;
};
struct AuditFilter {
  std::optional<int> submissionId, changedBy, departmentId;
  std::optional<TimePoint> from, to;
};

// ---- Repository interfaces ----
class IUserRepository {
 public:
  virtual ~IUserRepository() = default;
  virtual std::unique_ptr<User> findById(int id) = 0;
  virtual std::unique_ptr<User> findByUsername(const std::string& username) = 0;
  virtual int add(Role role, const NewUser& d) = 0;
  virtual void setActive(int id, bool active) = 0;
  virtual void setPasswordHash(int id, const std::string& hash) = 0;
  virtual LoginState loginState(int id) = 0;
  virtual void setLoginState(int id, const LoginState& s) = 0;
  virtual std::vector<std::unique_ptr<User>> listAll() = 0;
};
class ICourseRepository {
 public:
  virtual ~ICourseRepository() = default;
  virtual int addDepartment(const std::string& name) = 0;
  virtual std::vector<Department> listDepartments() = 0;
  virtual std::optional<Department> findDepartment(int id) = 0;
  virtual int addCourse(const Course& c) = 0;
  virtual std::optional<Course> findCourse(int id) = 0;
  virtual std::vector<Course> listCourses() = 0;
  virtual std::vector<Course> coursesForDepartment(int departmentId) = 0;
  virtual std::vector<Course> coursesForStudent(int studentId) = 0;
  virtual std::vector<Course> coursesForLecturer(int lecturerId) = 0;
  virtual void enrol(int studentId, int courseId) = 0;
  virtual void assignLecturer(int lecturerId, int courseId) = 0;
  virtual bool isEnrolled(int studentId, int courseId) = 0;
  virtual bool teaches(int lecturerId, int courseId) = 0;
  virtual std::vector<int> studentsInCourse(int courseId) = 0;
};
class IAssignmentRepository {
 public:
  virtual ~IAssignmentRepository() = default;
  virtual int add(const Assignment& a) = 0;
  virtual std::optional<Assignment> findById(int id) = 0;
  virtual std::vector<Assignment> listByCourse(int courseId) = 0;
  virtual int saveExtension(const Extension& e) = 0;  // one extension per student and assignment
  virtual std::optional<Extension> findExtension(int assignmentId, int studentId) = 0;
};
class ISubmissionRepository {
 public:
  virtual ~ISubmissionRepository() = default;
  virtual std::optional<Submission> findById(int id) = 0;
  virtual std::optional<Submission> findByStudentAndAssignment(int studentId, int assignmentId) = 0;
  virtual int add(const Submission& s) = 0;  // inserts the submission and its first version
  virtual int addVersion(int submissionId, const SubmissionVersion& v) = 0;
  virtual void setState(int id, SubmissionState s) = 0;
};
class IMarkRepository {
 public:
  virtual ~IMarkRepository() = default;
  virtual std::optional<Mark> find(int submissionId) = 0;
  virtual void upsert(const Mark& m) = 0;
  virtual void setApproval(int submissionId, int by, TimePoint at) = 0;
  virtual void setRelease(int submissionId, int by, TimePoint at) = 0;
  virtual void setFinalMark(int submissionId, double finalMark) = 0;
  virtual std::vector<SubmissionRow> rowsByAssignment(int assignmentId) = 0;
};
class IAuditRepository {
 public:
  virtual ~IAuditRepository() = default;
  virtual int add(const AuditEntry& e) = 0;
  virtual std::vector<AuditEntry> list(const AuditFilter& f) = 0;
};

// ---- SQLite implementations ----
class SqliteUserRepository : public IUserRepository {
 public:
  explicit SqliteUserRepository(Database& db) : db_(db) {}
  std::unique_ptr<User> findById(int id) override;
  std::unique_ptr<User> findByUsername(const std::string& username) override;
  int add(Role role, const NewUser& d) override;
  void setActive(int id, bool active) override;
  void setPasswordHash(int id, const std::string& hash) override;
  LoginState loginState(int id) override;
  void setLoginState(int id, const LoginState& s) override;
  std::vector<std::unique_ptr<User>> listAll() override;
 private:
  Database& db_;
};
class SqliteCourseRepository : public ICourseRepository {
 public:
  explicit SqliteCourseRepository(Database& db) : db_(db) {}
  int addDepartment(const std::string& name) override;
  std::vector<Department> listDepartments() override;
  std::optional<Department> findDepartment(int id) override;
  int addCourse(const Course& c) override;
  std::optional<Course> findCourse(int id) override;
  std::vector<Course> listCourses() override;
  std::vector<Course> coursesForDepartment(int departmentId) override;
  std::vector<Course> coursesForStudent(int studentId) override;
  std::vector<Course> coursesForLecturer(int lecturerId) override;
  void enrol(int studentId, int courseId) override;
  void assignLecturer(int lecturerId, int courseId) override;
  bool isEnrolled(int studentId, int courseId) override;
  bool teaches(int lecturerId, int courseId) override;
  std::vector<int> studentsInCourse(int courseId) override;
 private:
  std::vector<Course> query(const std::string& sql, int arg, bool hasArg);
  Database& db_;
};
class SqliteAssignmentRepository : public IAssignmentRepository {
 public:
  explicit SqliteAssignmentRepository(Database& db) : db_(db) {}
  int add(const Assignment& a) override;
  std::optional<Assignment> findById(int id) override;
  std::vector<Assignment> listByCourse(int courseId) override;
  int saveExtension(const Extension& e) override;
  std::optional<Extension> findExtension(int assignmentId, int studentId) override;
 private:
  Database& db_;
};
class SqliteSubmissionRepository : public ISubmissionRepository {
 public:
  explicit SqliteSubmissionRepository(Database& db) : db_(db) {}
  std::optional<Submission> findById(int id) override;
  std::optional<Submission> findByStudentAndAssignment(int studentId, int assignmentId) override;
  int add(const Submission& s) override;
  int addVersion(int submissionId, const SubmissionVersion& v) override;
  void setState(int id, SubmissionState s) override;
 private:
  std::optional<Submission> load(Statement& st);
  Database& db_;
};
class SqliteMarkRepository : public IMarkRepository {
 public:
  explicit SqliteMarkRepository(Database& db) : db_(db) {}
  std::optional<Mark> find(int submissionId) override;
  void upsert(const Mark& m) override;
  void setApproval(int submissionId, int by, TimePoint at) override;
  void setRelease(int submissionId, int by, TimePoint at) override;
  void setFinalMark(int submissionId, double finalMark) override;
  std::vector<SubmissionRow> rowsByAssignment(int assignmentId) override;
 private:
  Database& db_;
};
class SqliteAuditRepository : public IAuditRepository {
 public:
  explicit SqliteAuditRepository(Database& db) : db_(db) {}
  int add(const AuditEntry& e) override;
  std::vector<AuditEntry> list(const AuditFilter& f) override;
 private:
  Database& db_;
};

}  // namespace lms
