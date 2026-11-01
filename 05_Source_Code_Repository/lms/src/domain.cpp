#include "domain.hpp"

#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace lms {

std::string toString(Role r) {
  switch (r) {
    case Role::Student: return "STUDENT";
    case Role::Lecturer: return "LECTURER";
    case Role::Hod: return "HOD";
    case Role::Admin: return "ADMIN";
  }
  return "";
}
Role roleFromString(const std::string& s) {
  if (s == "STUDENT") return Role::Student;
  if (s == "LECTURER") return Role::Lecturer;
  if (s == "HOD") return Role::Hod;
  if (s == "ADMIN") return Role::Admin;
  throw ValidationError("Unknown role: " + s);
}
std::string displayName(Role r) {
  switch (r) {
    case Role::Student: return "Student";
    case Role::Lecturer: return "Lecturer";
    case Role::Hod: return "Head of Department";
    case Role::Admin: return "Administrator";
  }
  return "";
}
std::string toString(SubmissionState s) {
  switch (s) {
    case SubmissionState::Submitted: return "SUBMITTED";
    case SubmissionState::Late: return "LATE";
    case SubmissionState::Marked: return "MARKED";
    case SubmissionState::Moderated: return "MODERATED";
    case SubmissionState::Released: return "RELEASED";
  }
  return "";
}
SubmissionState stateFromString(const std::string& s) {
  for (auto st : {SubmissionState::Submitted, SubmissionState::Late, SubmissionState::Marked,
                  SubmissionState::Moderated, SubmissionState::Released})
    if (toString(st) == s) return st;
  throw ValidationError("Unknown submission state: " + s);
}
std::string toString(AuditAction a) {
  switch (a) {
    case AuditAction::InitialMark: return "INITIAL_MARK";
    case AuditAction::Remark: return "REMARK";
    case AuditAction::ModerationAdjust: return "MODERATION_ADJUST";
    case AuditAction::ReleasedCorrection: return "RELEASED_CORRECTION";
  }
  return "";
}
AuditAction auditActionFromString(const std::string& s) {
  for (auto a : {AuditAction::InitialMark, AuditAction::Remark, AuditAction::ModerationAdjust,
                 AuditAction::ReleasedCorrection})
    if (toString(a) == s) return a;
  throw ValidationError("Unknown audit action: " + s);
}

namespace {
bool in(Action a, std::initializer_list<Action> list) { return std::find(list.begin(), list.end(), a) != list.end(); }
}  // namespace

Role Student::role() const { return Role::Student; }
bool Student::allows(Action a) const {
  return in(a, {Action::ViewCourses, Action::SubmitWork, Action::ViewReleasedMarks, Action::ChangeOwnPassword});
}
Role Lecturer::role() const { return Role::Lecturer; }
bool Lecturer::allows(Action a) const {
  return in(a, {Action::ViewCourses, Action::CreateAssignment, Action::GrantExtension, Action::MarkSubmission,
                Action::ChangeOwnPassword});
}
Role HeadOfDepartment::role() const { return Role::Hod; }
bool HeadOfDepartment::allows(Action a) const {
  return in(a, {Action::ViewCourses, Action::ReviewDepartment, Action::ModerateMarks, Action::ReleaseMarks,
                Action::ViewAuditLog, Action::ChangeOwnPassword});
}
Role Administrator::role() const { return Role::Admin; }
bool Administrator::allows(Action a) const {
  return in(a, {Action::ManageAccounts, Action::ManageCourses, Action::EnrolAndAssign, Action::ViewCourses,
                Action::ViewAuditLog, Action::BackupDatabase, Action::ChangeOwnPassword});
}

std::unique_ptr<User> UserFactory::create(Role role, int id, const NewUser& d, bool active) {
  switch (role) {
    case Role::Student: return std::make_unique<Student>(id, d.username, d.fullName, d.passwordHash, active);
    case Role::Lecturer: return std::make_unique<Lecturer>(id, d.username, d.fullName, d.passwordHash, active);
    case Role::Admin: return std::make_unique<Administrator>(id, d.username, d.fullName, d.passwordHash, active);
    case Role::Hod:
      if (!d.departmentId) throw ValidationError("A head of department must have a department.");
      return std::make_unique<HeadOfDepartment>(id, d.username, d.fullName, d.passwordHash, active, *d.departmentId);
  }
  throw ValidationError("Unknown role.");
}

TimePoint effectiveDeadline(const Assignment& a, const std::optional<Extension>& e) {
  return e ? e->newDeadline : a.deadline;
}

const SubmissionVersion& Submission::currentVersion() const {
  if (versions.empty()) throw NotFoundError("The submission has no versions.");
  return *std::max_element(versions.begin(), versions.end(),
                           [](const SubmissionVersion& a, const SubmissionVersion& b) { return a.versionNo < b.versionNo; });
}

double roundHalfUp2(double v) { return std::floor(v * 100.0 + 0.5) / 100.0; }

PenaltyResult TenPercentPerDayPolicy::calculate(const PenaltyInput& in) const {
  int days = 0;
  if (in.submittedAt > in.effectiveDeadline) {
    const TimePoint delay = in.submittedAt - in.effectiveDeadline;
    days = static_cast<int>((delay + kDay - 1) / kDay);  // started 24-hour periods
  }
  if (days > maxDays_) return {days, roundHalfUp2(in.rawMark), 0.0};
  const double penalty = in.maxMark * rate_ / 100.0 * days;
  const double finalMark = roundHalfUp2(std::max(0.0, in.rawMark - penalty));
  return {days, roundHalfUp2(in.rawMark - finalMark), finalMark};
}

}  // namespace lms
