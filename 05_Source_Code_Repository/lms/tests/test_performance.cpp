// NFR-01 and NFR-02: response times with 800 student and 60 staff accounts stored.
#include <chrono>

#include "fixture.hpp"

using namespace lms;
using namespace lms::test;

namespace {
double millis(std::chrono::steady_clock::time_point a) {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - a).count();
}
}  // namespace

TEST(Performance, LoginListingAndSavingMeetTheTargetsWithEightHundredStudents) {
  TempDir tmp;
  Database db((tmp.path / "perf.db").string());  // a real file, as in deployment
  SystemClock clock;
  LibsodiumHasher hasher;  // production strength (Argon2id interactive)
  SqliteUserRepository users(db);
  SqliteCourseRepository courses(db);
  SqliteAssignmentRepository assignments(db);
  SqliteSubmissionRepository subs(db);
  SqliteMarkRepository marks(db);
  SqliteAuditRepository audit(db);
  SqliteUnitOfWork uow(db);
  TenPercentPerDayPolicy policy;
  MarkChangePublisher events;
  AuditObserver obs(audit);
  events.subscribe(&obs);
  AuthService auth(users, hasher, clock);
  MarkingService marking(assignments, subs, marks, courses, policy, events, uow, clock);
  AssignmentService assignmentSvc(assignments, courses, subs, marks, users, uow, clock);

  const std::string hash = hasher.hash("Passw0rd1");
  const int dept = courses.addDepartment("Computing");
  const int course = courses.addCourse({0, "CS101", "Intro", dept});
  int lecturerId = 0;
  {
    auto tx = uow.begin();
    for (int i = 0; i < 800; ++i) {
      const int id = users.add(Role::Student, NewUser{"student" + std::to_string(i), "Student " + std::to_string(i), hash, std::nullopt});
      courses.enrol(id, course);
    }
    for (int i = 0; i < 60; ++i) {
      const int id = users.add(Role::Lecturer, NewUser{"staff" + std::to_string(i), "Staff " + std::to_string(i), hash, std::nullopt});
      if (i == 0) { lecturerId = id; courses.assignLecturer(id, course); }
    }
    tx->commit();
  }
  ASSERT_EQ(users.listAll().size(), 860u);
  const TimePoint now = clock.now();
  Assignment a; a.courseId = course; a.title = "A"; a.description = "d"; a.deadline = now - kDay; a.maxMark = 100;
  a.createdBy = lecturerId; a.createdAt = now - 5 * kDay;
  const int aid = assignments.add(a);
  std::vector<int> subIds;
  {
    auto tx = uow.begin();
    for (int i = 0; i < 150; ++i) {
      Submission s; s.assignmentId = aid; s.studentId = i + 1; s.state = SubmissionState::Submitted;
      SubmissionVersion v; v.versionNo = 1; v.storedPath = "x"; v.originalName = "x"; v.sizeBytes = 10; v.submittedAt = now - 2 * kDay;
      s.versions.push_back(v);
      subIds.push_back(subs.add(s));
    }
    tx->commit();
  }

  auto t = std::chrono::steady_clock::now();
  auto user = auth.login("student799", "Passw0rd1");  // the last-created account
  const double loginMs = millis(t);
  ASSERT_TRUE(user);
  EXPECT_LT(loginMs, 2000.0) << "login took " << loginMs << " ms";

  auto lecturer = users.findById(lecturerId);
  t = std::chrono::steady_clock::now();
  const auto rows = assignmentSvc.listSubmissions(*lecturer, aid);
  const double listMs = millis(t);
  EXPECT_EQ(rows.size(), 150u);
  EXPECT_LT(listMs, 1000.0) << "listing took " << listMs << " ms";

  t = std::chrono::steady_clock::now();
  marking.mark(*lecturer, subIds[0], 80, "Good");
  const double saveMs = millis(t);
  EXPECT_LT(saveMs, 1000.0) << "saving a mark took " << saveMs << " ms";
 // CR1: one class extension covering all 800 enrolled students
  t = std::chrono::steady_clock::now();
  const ClassExtensionResult ce = assignmentSvc.grantClassExtension(*lecturer, aid, now + kDay, "Class extension");
  const double classMs = millis(t);
  EXPECT_EQ(ce.granted, 800);
  EXPECT_LT(classMs, 1000.0) << "class extension took " << classMs << " ms";
  std::cout << "[ perf ] class extension (800 students) " << classMs << " ms\n";
  std::cout << "[ perf ] login " << loginMs << " ms, list(150) " << listMs << " ms, save mark " << saveMs << " ms\n";
}
