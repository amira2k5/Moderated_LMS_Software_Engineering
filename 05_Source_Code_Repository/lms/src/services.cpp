#include "services.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>

namespace lms {

namespace fs = std::filesystem;

void requireAllowed(const User& actor, Action a) {
  if (!actor.active() || !actor.allows(a)) throw AccessDeniedError("You are not allowed to do this.");
}

namespace {
const int kMinReasonLength = 5;

std::string requireText(const std::string& value, const std::string& what, std::size_t maxLen) {
  const std::string t = trim(value);
  if (t.empty()) throw ValidationError("Please enter the " + what + ".");
  if (t.size() > maxLen) throw ValidationError("The " + what + " is too long (at most " + std::to_string(maxLen) + " characters).");
  return t;
}
std::string requireReason(const std::string& reason) {
  const std::string t = trim(reason);
  if (static_cast<int>(t.size()) < kMinReasonLength)
    throw ValidationError("Please give a reason of at least 5 characters.");
  return t;
}
void requireMarkInRange(double mark, int maxMark) {
  if (!std::isfinite(mark) || mark < 0 || mark > maxMark)
    throw ValidationError("The mark must be a number from 0 to " + std::to_string(maxMark) + ".");
}
Assignment requireAssignment(IAssignmentRepository& repo, int id) {
  auto a = repo.findById(id);
  if (!a) throw NotFoundError("That assignment does not exist.");
  return *a;
}
Submission requireSubmission(ISubmissionRepository& repo, int id) {
  auto s = repo.findById(id);
  if (!s) throw NotFoundError("That submission does not exist.");
  return *s;
}
Course requireCourse(ICourseRepository& repo, int id) {
  auto c = repo.findCourse(id);
  if (!c) throw NotFoundError("That course does not exist.");
  return *c;
}
}  // namespace

// ---------------- File store ----------------
namespace {
std::uintmax_t checkedFileSize(const fs::path& src) {  // validates a file offered for submission
  std::error_code ec;
  if (!fs::exists(src, ec) || !fs::is_regular_file(src, ec))
    throw ValidationError("The file was not found. Check the path and try again.");
  const auto size = fs::file_size(src, ec);
  if (ec || !std::ifstream(src, std::ios::binary).good()) throw ValidationError("The file cannot be read.");
  if (size == 0) throw ValidationError("The file is empty.");
  if (static_cast<std::int64_t>(size) > LocalFileStore::kMaxBytes) throw ValidationError("The file is larger than 5 MB.");
  return size;
}
}  // namespace

StoredFile LocalFileStore::store(const std::string& sourcePath) {
  std::error_code ec;
  const fs::path src(sourcePath);
  const auto size = checkedFileSize(src);
  static std::atomic<unsigned> counter{0};
  std::string clean;
  for (char c : src.filename().string()) clean += (std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '-') ? c : '_';
  const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now().time_since_epoch()).count();
  const fs::path dest = fs::path(folder_) / (std::to_string(stamp) + "_" + std::to_string(counter++) + "_" + clean);
  fs::create_directories(folder_, ec);
  fs::copy_file(src, dest, fs::copy_options::none, ec);
  if (ec) throw ValidationError("The file could not be saved. Check that the storage folder is available.");
  return {dest.string(), src.filename().string(), static_cast<std::int64_t>(size)};
}
void LocalFileStore::remove(const std::string& storedPath) noexcept {
  std::error_code ec;
  fs::remove(storedPath, ec);
}

// ---------------- Audit observer ----------------
void AuditObserver::onMarkChanged(const MarkChangedEvent& e) {
  AuditEntry a;
  a.submissionId = e.submissionId; a.changedBy = e.changedBy; a.changedAt = e.changedAt;
  a.action = e.action; a.oldValue = e.oldValue; a.newValue = e.newValue; a.reason = e.reason;
  audit_.add(a);
}

// ---------------- AuthService ----------------
std::unique_ptr<User> AuthService::login(const std::string& username, const std::string& password) {
  const std::string generic = "The username or password is not correct.";
  auto user = users_.findByUsername(trim(username));
  if (!user || !user->active()) throw AccessDeniedError(generic);
  LoginState st = users_.loginState(user->id());
  const TimePoint now = clock_.now();
  if (st.lockedUntil && *st.lockedUntil > now)
    throw AccessDeniedError("This account is locked after too many failed attempts. Try again after " +
                            formatLocal(*st.lockedUntil) + ".");
  if (!hasher_.verify(password, user->passwordHash())) {
    st.failedLogins++;
    if (st.failedLogins >= kMaxFailedLogins) { st.lockedUntil = now + kLockSeconds; st.failedLogins = 0; }
    users_.setLoginState(user->id(), st);
    throw AccessDeniedError(generic);
  }
  if (st.failedLogins != 0 || st.lockedUntil) users_.setLoginState(user->id(), LoginState{});
  return user;
}

void AuthService::changePassword(const User& actor, const std::string& oldPassword, const std::string& newPassword) {
  requireAllowed(actor, Action::ChangeOwnPassword);
  auto fresh = users_.findById(actor.id());
  if (!fresh || !hasher_.verify(oldPassword, fresh->passwordHash()))
    throw AccessDeniedError("The current password is not correct.");
  AdminService::validatePassword(newPassword, actor.username());
  users_.setPasswordHash(actor.id(), hasher_.hash(newPassword));
}

// ---------------- AdminService ----------------
void AdminService::validatePassword(const std::string& password, const std::string& username) {
  if (password.size() < 8) throw ValidationError("The password must have at least 8 characters.");
  if (password == username) throw ValidationError("The password must not be the same as the username.");
  const auto has = [&](int (*test)(int)) {
    return std::any_of(password.begin(), password.end(), [&](char c) { return test(static_cast<unsigned char>(c)) != 0; });
  };
  if (!has(std::isalpha) || !has(std::isdigit)) throw ValidationError("The password must contain at least one letter and one digit.");
}

namespace {
void validateUsername(const std::string& name) {
  if (name.size() < 3 || name.size() > 32) throw ValidationError("The username must have 3 to 32 characters.");
  const bool bad = std::any_of(name.begin(), name.end(), [](char c) {
    return !std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '.';
  });
  if (bad) throw ValidationError("The username may contain only letters, digits, underscore and full stop.");
}
}  // namespace

int AdminService::createUser(const User& actor, Role role, const std::string& username, const std::string& fullName,
                             const std::string& password, std::optional<int> departmentId) {
  requireAllowed(actor, Action::ManageAccounts);
  const std::string name = trim(username);
  validateUsername(name);
  const std::string full = requireText(fullName, "full name", 80);
  validatePassword(password, name);
  if (users_.findByUsername(name)) throw ValidationError("That username is already used.");
  if (role == Role::Hod) {
    if (!departmentId || !courses_.findDepartment(*departmentId)) throw ValidationError("Choose an existing department for the head of department.");
    for (const auto& u : users_.listAll())
      if (auto* h = dynamic_cast<const HeadOfDepartment*>(u.get()); h && h->departmentId() == *departmentId)
        throw ValidationError("That department already has a head of department.");
  } else {
    departmentId.reset();
  }
  NewUser nu{name, full, hasher_.hash(password), departmentId};
  UserFactory::create(role, 0, nu);  // validates the combination of role and data
  return users_.add(role, nu);
}

void AdminService::setActive(const User& actor, int userId, bool active) {
  requireAllowed(actor, Action::ManageAccounts);
  if (!users_.findById(userId)) throw NotFoundError("That user does not exist.");
  if (userId == actor.id() && !active) throw RuleViolation("You cannot deactivate your own account.");
  users_.setActive(userId, active);
}
int AdminService::createDepartment(const User& actor, const std::string& name) {
  requireAllowed(actor, Action::ManageCourses);
  return courses_.addDepartment(requireText(name, "department name", 80));
}
int AdminService::createCourse(const User& actor, const std::string& code, const std::string& title, int departmentId) {
  requireAllowed(actor, Action::ManageCourses);
  if (!courses_.findDepartment(departmentId)) throw NotFoundError("That department does not exist.");
  return courses_.addCourse({0, requireText(code, "course code", 20), requireText(title, "course title", 100), departmentId});
}
void AdminService::enrol(const User& actor, int studentId, int courseId) {
  requireAllowed(actor, Action::EnrolAndAssign);
  auto u = users_.findById(studentId);
  if (!u || u->role() != Role::Student || !u->active()) throw ValidationError("Choose an active student account.");
  requireCourse(courses_, courseId);
  if (courses_.isEnrolled(studentId, courseId)) throw ValidationError("That student is already enrolled in the course.");
  courses_.enrol(studentId, courseId);
}
void AdminService::assignLecturer(const User& actor, int lecturerId, int courseId) {
  requireAllowed(actor, Action::EnrolAndAssign);
  auto u = users_.findById(lecturerId);
  if (!u || u->role() != Role::Lecturer || !u->active()) throw ValidationError("Choose an active lecturer account.");
  requireCourse(courses_, courseId);
  if (courses_.teaches(lecturerId, courseId)) throw ValidationError("That lecturer is already assigned to the course.");
  courses_.assignLecturer(lecturerId, courseId);
}
std::vector<std::unique_ptr<User>> AdminService::listUsers(const User& actor) {
  requireAllowed(actor, Action::ManageAccounts);
  return users_.listAll();
}
std::vector<Department> AdminService::listDepartments(const User& actor) {
  requireAllowed(actor, Action::ViewCourses);
  return courses_.listDepartments();
}
std::vector<Course> AdminService::listCourses(const User& actor) {
  requireAllowed(actor, Action::ViewCourses);
  return courses_.listCourses();
}
void AdminService::backup(const User& actor, const std::string& destination) {
  requireAllowed(actor, Action::BackupDatabase);
  if (trim(destination).empty()) throw ValidationError("Please enter the name of the backup file.");
  db_.backupTo(trim(destination));
}

// ---------------- AssignmentService ----------------
std::vector<Course> AssignmentService::coursesFor(const User& actor) {
  requireAllowed(actor, Action::ViewCourses);
  switch (actor.role()) {
    case Role::Student: return courses_.coursesForStudent(actor.id());
    case Role::Lecturer: return courses_.coursesForLecturer(actor.id());
    case Role::Hod: return courses_.coursesForDepartment(static_cast<const HeadOfDepartment&>(actor).departmentId());
    case Role::Admin: return courses_.listCourses();
  }
  return {};
}

void AssignmentService::requireCourseAccess(const User& actor, int courseId) {
  const Course c = requireCourse(courses_, courseId);
  bool ok = false;
  switch (actor.role()) {
    case Role::Student: ok = courses_.isEnrolled(actor.id(), courseId); break;
    case Role::Lecturer: ok = courses_.teaches(actor.id(), courseId); break;
    case Role::Hod: ok = static_cast<const HeadOfDepartment&>(actor).departmentId() == c.departmentId; break;
    case Role::Admin: ok = true; break;
  }
  if (!ok) throw AccessDeniedError("You do not have access to that course.");
}

int AssignmentService::createAssignment(const User& actor, int courseId, const std::string& title,
                                        const std::string& description, TimePoint deadline, int maxMark) {
  requireAllowed(actor, Action::CreateAssignment);
  requireCourseAccess(actor, courseId);
  Assignment a;
  a.courseId = courseId;
  a.title = requireText(title, "assignment title", 120);
  a.description = requireText(description, "description", 2000);
  if (maxMark < 1 || maxMark > 1000) throw ValidationError("The maximum mark must be a whole number from 1 to 1000.");
  a.maxMark = maxMark;
  if (deadline <= clock_.now()) throw ValidationError("The deadline must be in the future.");
  a.deadline = deadline;
  a.createdBy = actor.id();
  a.createdAt = clock_.now();
  return assignments_.add(a);
}

void AssignmentService::grantExtension(const User& actor, int assignmentId, int studentId, TimePoint newDeadline,
                                       const std::string& reason) {
  requireAllowed(actor, Action::GrantExtension);
  const Assignment a = requireAssignment(assignments_, assignmentId);
  requireCourseAccess(actor, a.courseId);
  if (!courses_.isEnrolled(studentId, a.courseId)) throw ValidationError("That student is not enrolled in this course.");
  if (newDeadline <= a.deadline) throw ValidationError("The extended deadline must be later than the assignment deadline.");
  Extension e;
  e.assignmentId = assignmentId; e.studentId = studentId; e.newDeadline = newDeadline;
  e.reason = requireReason(reason); e.grantedBy = actor.id(); e.grantedAt = clock_.now();
  assignments_.saveExtension(e);
}

ClassExtensionResult AssignmentService::grantClassExtension(const User& actor, int assignmentId, TimePoint newDeadline,
                                                            const std::string& reason) {
  requireAllowed(actor, Action::GrantExtension);
  const Assignment a = requireAssignment(assignments_, assignmentId);
  requireCourseAccess(actor, a.courseId);
  if (newDeadline <= a.deadline) throw ValidationError("The extended deadline must be later than the assignment deadline.");
  const std::string why = requireReason(reason);
  const std::vector<int> students = courses_.studentsInCourse(a.courseId);
  if (students.empty()) throw ValidationError("No students are enrolled in this course.");
  ClassExtensionResult result;
  auto tx = uow_.begin();  // all students or none
  for (int studentId : students) {
    const auto existing = assignments_.findExtension(assignmentId, studentId);
    if (existing && existing->newDeadline >= newDeadline) { result.kept++; continue; }
    Extension e;
    e.assignmentId = assignmentId; e.studentId = studentId; e.newDeadline = newDeadline;
    e.reason = why; e.grantedBy = actor.id(); e.grantedAt = clock_.now();
    assignments_.saveExtension(e);
    result.granted++;
  }
  tx->commit();
  return result;
}

std::vector<AssignmentRow> AssignmentService::assignmentsFor(const User& actor, int courseId) {
  requireAllowed(actor, Action::ViewCourses);
  requireCourseAccess(actor, courseId);
  std::vector<AssignmentRow> rows;
  for (const auto& a : assignments_.listByCourse(courseId)) {
    AssignmentRow r{a, std::nullopt, std::nullopt};
    if (actor.role() == Role::Student) {
      if (auto s = subs_.findByStudentAndAssignment(actor.id(), a.id)) r.state = s->state;
      if (auto e = assignments_.findExtension(a.id, actor.id())) r.extendedTo = e->newDeadline;
    }
    rows.push_back(r);
  }
  return rows;
}

std::vector<SubmissionRow> AssignmentService::listSubmissions(const User& lecturer, int assignmentId) {
  requireAllowed(lecturer, Action::MarkSubmission);
  const Assignment a = requireAssignment(assignments_, assignmentId);
  requireCourseAccess(lecturer, a.courseId);
  return marks_.rowsByAssignment(assignmentId);
}

std::vector<StudentRef> AssignmentService::studentsIn(const User& staff, int courseId) {
  requireAllowed(staff, Action::ViewCourses);
  if (staff.role() == Role::Student) throw AccessDeniedError("You are not allowed to do this.");
  requireCourseAccess(staff, courseId);
  std::vector<StudentRef> out;
  for (int id : courses_.studentsInCourse(courseId))
    if (auto u = users_.findById(id)) out.push_back({id, u->username(), u->fullName()});
  return out;
}

// ---------------- SubmissionService ----------------
Receipt SubmissionService::submit(const User& student, int assignmentId, const std::string& filePath) {
  requireAllowed(student, Action::SubmitWork);
  const Assignment a = requireAssignment(assignments_, assignmentId);
  if (!courses_.isEnrolled(student.id(), a.courseId))
    throw AccessDeniedError("You are not enrolled in the course for this assignment.");
  const TimePoint now = clock_.now();
  const TimePoint effective = effectiveDeadline(a, assignments_.findExtension(assignmentId, student.id()));
  const bool onTime = now <= effective;
  const int daysLate = onTime ? 0 : static_cast<int>((now - effective + kDay - 1) / kDay);
  auto existing = subs_.findByStudentAndAssignment(student.id(), assignmentId);
  if (existing) {  // BR-7: only an on-time submission that is still Submitted can be replaced
    if (existing->state == SubmissionState::Late) throw RuleViolation("A late submission cannot be replaced.");
    if (existing->state != SubmissionState::Submitted)
      throw RuleViolation("Your work has already been marked, so it can no longer be changed.");
    if (!onTime) throw RuleViolation("The deadline has passed, so you can no longer resubmit.");
  }
  const StoredFile file = files_.store(filePath);
  try {
    auto tx = uow_.begin();
    SubmissionVersion v;
    v.storedPath = file.storedPath; v.originalName = file.originalName; v.sizeBytes = file.sizeBytes; v.submittedAt = now;
    Receipt r{0, 1, onTime ? SubmissionState::Submitted : SubmissionState::Late, daysLate, now};
    if (existing) {
      v.versionNo = existing->currentVersion().versionNo + 1;
      subs_.addVersion(existing->id, v);
      r.submissionId = existing->id; r.versionNo = v.versionNo;
    } else {
      Submission s;
      s.assignmentId = assignmentId; s.studentId = student.id(); s.state = r.state;
      v.versionNo = 1; s.versions.push_back(v);
      r.submissionId = subs_.add(s);
    }
    tx->commit();
    return r;
  } catch (...) {
    files_.remove(file.storedPath);
    throw;
  }
}

std::optional<ReleasedMark> SubmissionService::releasedMark(const User& student, int assignmentId) {
  requireAllowed(student, Action::ViewReleasedMarks);
  auto s = subs_.findByStudentAndAssignment(student.id(), assignmentId);
  if (!s || s->state != SubmissionState::Released) return std::nullopt;  // students never see unreleased marks
  auto m = marks_.find(s->id);
  if (!m || !m->releasedAt) return std::nullopt;
  return ReleasedMark{m->finalMark, m->rawMark, m->daysLate, m->feedback, *m->releasedAt};
}

// ---------------- MarkingService ----------------
MarkingService::Context MarkingService::load(const User& lecturer, int submissionId) {
  Context c;
  c.sub = requireSubmission(subs_, submissionId);
  c.assignment = requireAssignment(assignments_, c.sub.assignmentId);
  if (!courses_.teaches(lecturer.id(), c.assignment.courseId))
    throw AccessDeniedError("You do not teach the course for this assignment.");
  c.effective = effectiveDeadline(c.assignment, assignments_.findExtension(c.assignment.id, c.sub.studentId));
  return c;
}

PenaltyResult MarkingService::penaltyFor(const Context& c, double rawMark) const {
  return policy_.calculate({static_cast<double>(c.assignment.maxMark), rawMark, c.effective,
                            c.sub.currentVersion().submittedAt});
}

MarkResult MarkingService::mark(const User& lecturer, int submissionId, double rawMark, const std::string& feedback) {
  requireAllowed(lecturer, Action::MarkSubmission);
  const Context c = load(lecturer, submissionId);
  if (c.sub.state == SubmissionState::Marked)
    throw RuleViolation("This submission is already marked. Use re-mark to change the mark.");
  if (c.sub.state != SubmissionState::Submitted && c.sub.state != SubmissionState::Late)
    throw RuleViolation("This mark has been checked by the head of department. Only the head of department can change it.");
  if (clock_.now() <= c.effective)
    throw RuleViolation("Marking opens after the deadline (" + formatLocal(c.effective) + ").");
  requireMarkInRange(rawMark, c.assignment.maxMark);
  const std::string fb = requireText(feedback, "written feedback", 4000);
  const PenaltyResult p = penaltyFor(c, rawMark);
  Mark m;
  m.submissionId = submissionId; m.rawMark = rawMark; m.daysLate = p.daysLate; m.finalMark = p.finalMark;
  m.feedback = fb; m.markedBy = lecturer.id(); m.markedAt = clock_.now();
  auto tx = uow_.begin();
  marks_.upsert(m);
  subs_.setState(submissionId, SubmissionState::Marked);
  events_.publish({submissionId, lecturer.id(), m.markedAt, std::nullopt, p.finalMark, "initial marking",
                   AuditAction::InitialMark});
  tx->commit();
  return {submissionId, rawMark, p};
}

MarkResult MarkingService::remark(const User& lecturer, int submissionId, double rawMark, const std::string& reason,
                                  const std::optional<std::string>& newFeedback) {
  requireAllowed(lecturer, Action::MarkSubmission);
  const Context c = load(lecturer, submissionId);
  if (c.sub.state != SubmissionState::Marked)
    throw RuleViolation("Only a marked submission that the head of department has not yet approved can be re-marked.");
  requireMarkInRange(rawMark, c.assignment.maxMark);
  const std::string why = requireReason(reason);
  auto old = marks_.find(submissionId);
  if (!old) throw NotFoundError("This submission has no mark yet.");
  const PenaltyResult p = penaltyFor(c, rawMark);
  Mark m = *old;
  m.rawMark = rawMark; m.daysLate = p.daysLate; m.finalMark = p.finalMark;
  m.markedBy = lecturer.id(); m.markedAt = clock_.now();
  if (newFeedback) m.feedback = requireText(*newFeedback, "written feedback", 4000);
  auto tx = uow_.begin();
  marks_.upsert(m);
  events_.publish({submissionId, lecturer.id(), m.markedAt, old->finalMark, p.finalMark, why, AuditAction::Remark});
  tx->commit();
  return {submissionId, rawMark, p};
}

// ---------------- ModerationService ----------------
Assignment ModerationService::assignmentInDepartment(const User& hod, int assignmentId) {
  const Assignment a = requireAssignment(assignments_, assignmentId);
  const Course c = requireCourse(courses_, a.courseId);
  if (c.departmentId != static_cast<const HeadOfDepartment&>(hod).departmentId())
    throw AccessDeniedError("That course is not in your department.");
  return a;
}
Submission ModerationService::submissionInDepartment(const User& hod, int submissionId, Assignment* out) {
  const Submission s = requireSubmission(subs_, submissionId);
  const Assignment a = assignmentInDepartment(hod, s.assignmentId);
  if (out) *out = a;
  return s;
}

std::vector<SubmissionRow> ModerationService::listForModeration(const User& hod, int assignmentId) {
  requireAllowed(hod, Action::ReviewDepartment);
  assignmentInDepartment(hod, assignmentId);
  return marks_.rowsByAssignment(assignmentId);
}

void ModerationService::changeFinalMark(const User& hod, const Submission& s, const Assignment& a, double newFinal,
                                        const std::string& reason, AuditAction action) {
  requireMarkInRange(newFinal, a.maxMark);
  const std::string why = requireReason(reason);
  auto m = marks_.find(s.id);
  if (!m) throw NotFoundError("This submission has no mark yet.");
  auto tx = uow_.begin();
  marks_.setFinalMark(s.id, newFinal);
  events_.publish({s.id, hod.id(), clock_.now(), m->finalMark, newFinal, why, action});
  tx->commit();
}

void ModerationService::adjustMark(const User& hod, int submissionId, double newFinalMark, const std::string& reason) {
  requireAllowed(hod, Action::ModerateMarks);
  Assignment a;
  const Submission s = submissionInDepartment(hod, submissionId, &a);
  if (s.state != SubmissionState::Marked)
    throw RuleViolation("A mark can be adjusted only before it is approved. Use the correction function for released marks.");
  changeFinalMark(hod, s, a, newFinalMark, reason, AuditAction::ModerationAdjust);
}

void ModerationService::correctReleasedMark(const User& hod, int submissionId, double newFinalMark,
                                            const std::string& reason) {
  requireAllowed(hod, Action::ModerateMarks);
  Assignment a;
  const Submission s = submissionInDepartment(hod, submissionId, &a);
  if (s.state != SubmissionState::Released) throw RuleViolation("Only a released mark can be corrected this way.");
  changeFinalMark(hod, s, a, newFinalMark, reason, AuditAction::ReleasedCorrection);
}

BatchResult ModerationService::approve(const User& hod, const std::vector<int>& submissionIds) {
  requireAllowed(hod, Action::ModerateMarks);
  BatchResult r;
  auto tx = uow_.begin();  // all or nothing
  for (int id : submissionIds) {
    const Submission s = submissionInDepartment(hod, id);
    if (s.state != SubmissionState::Marked) { r.skipped++; continue; }
    marks_.setApproval(id, hod.id(), clock_.now());
    subs_.setState(id, SubmissionState::Moderated);
    r.done++;
  }
  tx->commit();
  return r;
}

BatchResult ModerationService::release(const User& hod, const std::vector<int>& submissionIds) {
  requireAllowed(hod, Action::ReleaseMarks);
  BatchResult r;
  auto tx = uow_.begin();
  for (int id : submissionIds) {
    const Submission s = submissionInDepartment(hod, id);
    if (s.state != SubmissionState::Moderated) { r.skipped++; continue; }
    marks_.setRelease(id, hod.id(), clock_.now());
    subs_.setState(id, SubmissionState::Released);
    r.done++;
  }
  tx->commit();
  return r;
}

// ---------------- AuditService ----------------
std::vector<AuditEntry> AuditService::list(const User& actor, AuditFilter filter) {
  requireAllowed(actor, Action::ViewAuditLog);
  if (actor.role() == Role::Hod) filter.departmentId = static_cast<const HeadOfDepartment&>(actor).departmentId();
  return audit_.list(filter);
}

}  // namespace lms
