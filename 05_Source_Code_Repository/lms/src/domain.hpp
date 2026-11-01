// Domain layer: users and roles, entities, the late-penalty Strategy,
// mark-change events (Observer) and the user Factory.
#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "common.hpp"

namespace lms {

enum class Role { Student, Lecturer, Hod, Admin };
enum class Action {
  ManageAccounts, ManageCourses, EnrolAndAssign, ViewCourses, CreateAssignment, GrantExtension,
  SubmitWork, MarkSubmission, ReviewDepartment, ModerateMarks, ReleaseMarks, ViewReleasedMarks,
  ViewAuditLog, BackupDatabase, ChangeOwnPassword
};
enum class SubmissionState { Submitted, Late, Marked, Moderated, Released };
enum class AuditAction { InitialMark, Remark, ModerationAdjust, ReleasedCorrection };

std::string toString(Role r);
Role roleFromString(const std::string& s);
std::string toString(SubmissionState s);
SubmissionState stateFromString(const std::string& s);
std::string toString(AuditAction a);
AuditAction auditActionFromString(const std::string& s);
std::string displayName(Role r);

// ---- Users (abstract base with polymorphic permissions) ----
class User {
 public:
  User(int id, std::string username, std::string fullName, std::string passwordHash, bool active)
      : id_(id), username_(std::move(username)), fullName_(std::move(fullName)),
        passwordHash_(std::move(passwordHash)), active_(active) {}
  virtual ~User() = default;
  virtual Role role() const = 0;
  virtual bool allows(Action a) const = 0;
  int id() const { return id_; }
  const std::string& username() const { return username_; }
  const std::string& fullName() const { return fullName_; }
  const std::string& passwordHash() const { return passwordHash_; }
  bool active() const { return active_; }
 private:
  int id_;
  std::string username_, fullName_, passwordHash_;
  bool active_;
};

class Student : public User { public: using User::User; Role role() const override; bool allows(Action a) const override; };
class Lecturer : public User { public: using User::User; Role role() const override; bool allows(Action a) const override; };
class Administrator : public User { public: using User::User; Role role() const override; bool allows(Action a) const override; };
class HeadOfDepartment : public User {
 public:
  HeadOfDepartment(int id, std::string username, std::string fullName, std::string hash, bool active, int departmentId)
      : User(id, std::move(username), std::move(fullName), std::move(hash), active), departmentId_(departmentId) {}
  Role role() const override;
  bool allows(Action a) const override;
  int departmentId() const { return departmentId_; }
 private:
  int departmentId_;
};

struct NewUser {
  std::string username, fullName, passwordHash;
  std::optional<int> departmentId;  // required for a head of department
};

class UserFactory {
 public:
  static std::unique_ptr<User> create(Role role, int id, const NewUser& d, bool active = true);
};

// ---- Entities ----
struct Department { int id = 0; std::string name; };
struct Course { int id = 0; std::string code, title; int departmentId = 0; };
struct Assignment {
  int id = 0, courseId = 0;
  std::string title, description;
  TimePoint deadline = 0;
  int maxMark = 0;
  int createdBy = 0;
  TimePoint createdAt = 0;
};
struct Extension {
  int id = 0, assignmentId = 0, studentId = 0;
  TimePoint newDeadline = 0;
  std::string reason;
  int grantedBy = 0;
  TimePoint grantedAt = 0;
};
// BR-1: the extension deadline if the student has one, otherwise the assignment deadline.
TimePoint effectiveDeadline(const Assignment& a, const std::optional<Extension>& e);

struct SubmissionVersion {
  int id = 0, versionNo = 0;
  std::string storedPath, originalName;
  std::int64_t sizeBytes = 0;
  TimePoint submittedAt = 0;
};
struct Submission {
  int id = 0, assignmentId = 0, studentId = 0;
  SubmissionState state = SubmissionState::Submitted;
  std::vector<SubmissionVersion> versions;
  const SubmissionVersion& currentVersion() const;  // highest version number
};
struct Mark {
  int submissionId = 0;
  double rawMark = 0;
  int daysLate = 0;
  double finalMark = 0;
  std::string feedback;
  int markedBy = 0;
  TimePoint markedAt = 0;
  std::optional<int> approvedBy;
  std::optional<TimePoint> approvedAt;
  std::optional<int> releasedBy;
  std::optional<TimePoint> releasedAt;
  double penalty() const { return rawMark - finalMark; }
};
struct AuditEntry {
  int id = 0, submissionId = 0, changedBy = 0;
  TimePoint changedAt = 0;
  AuditAction action = AuditAction::InitialMark;
  std::optional<double> oldValue;
  double newValue = 0;
  std::string reason;
};

// ---- Strategy: late-penalty rule (BR-2 to BR-4) ----
struct PenaltyInput { double maxMark; double rawMark; TimePoint effectiveDeadline; TimePoint submittedAt; };
struct PenaltyResult { int daysLate; double penalty; double finalMark; };

class ILatePenaltyPolicy {
 public:
  virtual ~ILatePenaltyPolicy() = default;
  virtual PenaltyResult calculate(const PenaltyInput& in) const = 0;
};

class TenPercentPerDayPolicy : public ILatePenaltyPolicy {
 public:
  explicit TenPercentPerDayPolicy(int ratePercent = 10, int maxDays = 3) : rate_(ratePercent), maxDays_(maxDays) {}
  PenaltyResult calculate(const PenaltyInput& in) const override;
 private:
  int rate_, maxDays_;
};

double roundHalfUp2(double v);

// ---- Observer: mark-change events ----
struct MarkChangedEvent {
  int submissionId = 0, changedBy = 0;
  TimePoint changedAt = 0;
  std::optional<double> oldValue;
  double newValue = 0;
  std::string reason;
  AuditAction action = AuditAction::InitialMark;
};
class IMarkChangeObserver {
 public:
  virtual ~IMarkChangeObserver() = default;
  virtual void onMarkChanged(const MarkChangedEvent& e) = 0;
};
class MarkChangePublisher {
 public:
  void subscribe(IMarkChangeObserver* o) { observers_.push_back(o); }
  void publish(const MarkChangedEvent& e) { for (auto* o : observers_) o->onMarkChanged(e); }
 private:
  std::vector<IMarkChangeObserver*> observers_;
};

}  // namespace lms
