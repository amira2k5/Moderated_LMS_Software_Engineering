// Application layer: one service per group of use cases. Services depend only on
// interfaces (repositories, penalty policy, hasher, clock, file store, unit of work).
#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "data.hpp"
#include "domain.hpp"
#include "security.hpp"

namespace lms {

void requireAllowed(const User& actor, Action a);  // throws AccessDeniedError

// ---- File storage ----
struct StoredFile { std::string storedPath, originalName; std::int64_t sizeBytes = 0; };
class IFileStore {
 public:
  virtual ~IFileStore() = default;
  virtual StoredFile store(const std::string& sourcePath) = 0;  // validates and copies; throws ValidationError
  virtual void remove(const std::string& storedPath) noexcept = 0;
};
class LocalFileStore : public IFileStore {
 public:
  explicit LocalFileStore(std::string folder) : folder_(std::move(folder)) {}
  StoredFile store(const std::string& sourcePath) override;
  void remove(const std::string& storedPath) noexcept override;
  static constexpr std::int64_t kMaxBytes = 5 * 1024 * 1024;
 private:
  std::string folder_;
};

// ---- Observer that writes every mark change to the audit log (FR-13) ----
class AuditObserver : public IMarkChangeObserver {
 public:
  explicit AuditObserver(IAuditRepository& audit) : audit_(audit) {}
  void onMarkChanged(const MarkChangedEvent& e) override;
 private:
  IAuditRepository& audit_;
};

// ---- Result types ----
struct Receipt { int submissionId; int versionNo; SubmissionState state; int daysLate; TimePoint submittedAt; };
struct MarkResult { int submissionId; double rawMark; PenaltyResult penalty; };
struct BatchResult { int done = 0; int skipped = 0; };
struct ReleasedMark { double finalMark; double rawMark; int daysLate; std::string feedback; TimePoint releasedAt; };
struct StudentRef { int id; std::string username, fullName; };
struct AssignmentRow {
  Assignment assignment;
  std::optional<SubmissionState> state;     // the student's own submission, if any
  std::optional<TimePoint> extendedTo;      // the student's extended deadline, if any (CR1)
};
struct ClassExtensionResult { int granted = 0; int kept = 0; };  // kept: students who already had a later extension

// ---- Services ----
class AuthService {
 public:
  static constexpr int kMaxFailedLogins = 5;
  static constexpr TimePoint kLockSeconds = 300;
  AuthService(IUserRepository& users, const IPasswordHasher& hasher, const Clock& clock)
      : users_(users), hasher_(hasher), clock_(clock) {}
  std::unique_ptr<User> login(const std::string& username, const std::string& password);
  void changePassword(const User& actor, const std::string& oldPassword, const std::string& newPassword);
 private:
  IUserRepository& users_;
  const IPasswordHasher& hasher_;
  const Clock& clock_;
};

class AdminService {
 public:
  AdminService(IUserRepository& users, ICourseRepository& courses, const IPasswordHasher& hasher, Database& db)
      : users_(users), courses_(courses), hasher_(hasher), db_(db) {}
  int createUser(const User& actor, Role role, const std::string& username, const std::string& fullName,
                 const std::string& password, std::optional<int> departmentId);
  void setActive(const User& actor, int userId, bool active);
  int createDepartment(const User& actor, const std::string& name);
  int createCourse(const User& actor, const std::string& code, const std::string& title, int departmentId);
  void enrol(const User& actor, int studentId, int courseId);
  void assignLecturer(const User& actor, int lecturerId, int courseId);
  std::vector<std::unique_ptr<User>> listUsers(const User& actor);
  std::vector<Department> listDepartments(const User& actor);
  std::vector<Course> listCourses(const User& actor);
  void backup(const User& actor, const std::string& destination);
  static void validatePassword(const std::string& password, const std::string& username);
 private:
  IUserRepository& users_;
  ICourseRepository& courses_;
  const IPasswordHasher& hasher_;
  Database& db_;
};

class AssignmentService {
 public:
  AssignmentService(IAssignmentRepository& assignments, ICourseRepository& courses, ISubmissionRepository& subs,
                    IMarkRepository& marks, IUserRepository& users, IUnitOfWork& uow, const Clock& clock)
      : assignments_(assignments), courses_(courses), subs_(subs), marks_(marks), users_(users), uow_(uow),
        clock_(clock) {}
  std::vector<Course> coursesFor(const User& actor);
  int createAssignment(const User& actor, int courseId, const std::string& title, const std::string& description,
                       TimePoint deadline, int maxMark);
  void grantExtension(const User& actor, int assignmentId, int studentId, TimePoint newDeadline,
                      const std::string& reason);
  // CR1: one extension for every enrolled student; never shortens a later extension a student already has.
  ClassExtensionResult grantClassExtension(const User& actor, int assignmentId, TimePoint newDeadline,
                                           const std::string& reason);
  std::vector<AssignmentRow> assignmentsFor(const User& actor, int courseId);
  std::vector<SubmissionRow> listSubmissions(const User& lecturer, int assignmentId);
  std::vector<StudentRef> studentsIn(const User& staff, int courseId);
 private:
  void requireCourseAccess(const User& actor, int courseId);
  IAssignmentRepository& assignments_;
  ICourseRepository& courses_;
  ISubmissionRepository& subs_;
  IMarkRepository& marks_;
  IUserRepository& users_;
  IUnitOfWork& uow_;
  const Clock& clock_;
};

class SubmissionService {
 public:
  SubmissionService(IAssignmentRepository& assignments, ISubmissionRepository& subs, ICourseRepository& courses,
                    IMarkRepository& marks, IFileStore& files, IUnitOfWork& uow, const Clock& clock)
      : assignments_(assignments), subs_(subs), courses_(courses), marks_(marks), files_(files), uow_(uow),
        clock_(clock) {}
  Receipt submit(const User& student, int assignmentId, const std::string& filePath);
  std::optional<ReleasedMark> releasedMark(const User& student, int assignmentId);
 private:
  IAssignmentRepository& assignments_;
  ISubmissionRepository& subs_;
  ICourseRepository& courses_;
  IMarkRepository& marks_;
  IFileStore& files_;
  IUnitOfWork& uow_;
  const Clock& clock_;
};

class MarkingService {
 public:
  MarkingService(IAssignmentRepository& assignments, ISubmissionRepository& subs, IMarkRepository& marks,
                 ICourseRepository& courses, const ILatePenaltyPolicy& policy, MarkChangePublisher& events,
                 IUnitOfWork& uow, const Clock& clock)
      : assignments_(assignments), subs_(subs), marks_(marks), courses_(courses), policy_(policy),
        events_(events), uow_(uow), clock_(clock) {}
  MarkResult mark(const User& lecturer, int submissionId, double rawMark, const std::string& feedback);
  MarkResult remark(const User& lecturer, int submissionId, double rawMark, const std::string& reason,
                    const std::optional<std::string>& newFeedback = std::nullopt);
 private:
  struct Context { Submission sub; Assignment assignment; TimePoint effective; };
  Context load(const User& lecturer, int submissionId);
  PenaltyResult penaltyFor(const Context& c, double rawMark) const;
  IAssignmentRepository& assignments_;
  ISubmissionRepository& subs_;
  IMarkRepository& marks_;
  ICourseRepository& courses_;
  const ILatePenaltyPolicy& policy_;
  MarkChangePublisher& events_;
  IUnitOfWork& uow_;
  const Clock& clock_;
};

class ModerationService {
 public:
  ModerationService(ICourseRepository& courses, IAssignmentRepository& assignments, ISubmissionRepository& subs,
                    IMarkRepository& marks, MarkChangePublisher& events, IUnitOfWork& uow, const Clock& clock)
      : courses_(courses), assignments_(assignments), subs_(subs), marks_(marks), events_(events), uow_(uow),
        clock_(clock) {}
  std::vector<SubmissionRow> listForModeration(const User& hod, int assignmentId);
  void adjustMark(const User& hod, int submissionId, double newFinalMark, const std::string& reason);
  BatchResult approve(const User& hod, const std::vector<int>& submissionIds);
  BatchResult release(const User& hod, const std::vector<int>& submissionIds);
  void correctReleasedMark(const User& hod, int submissionId, double newFinalMark, const std::string& reason);
 private:
  Assignment assignmentInDepartment(const User& hod, int assignmentId);
  Submission submissionInDepartment(const User& hod, int submissionId, Assignment* out = nullptr);
  void changeFinalMark(const User& hod, const Submission& s, const Assignment& a, double newFinal,
                       const std::string& reason, AuditAction action);
  ICourseRepository& courses_;
  IAssignmentRepository& assignments_;
  ISubmissionRepository& subs_;
  IMarkRepository& marks_;
  MarkChangePublisher& events_;
  IUnitOfWork& uow_;
  const Clock& clock_;
};

class AuditService {
 public:
  explicit AuditService(IAuditRepository& audit) : audit_(audit) {}
  std::vector<AuditEntry> list(const User& actor, AuditFilter filter);  // a HoD sees only their department
 private:
  IAuditRepository& audit_;
};

}  // namespace lms
