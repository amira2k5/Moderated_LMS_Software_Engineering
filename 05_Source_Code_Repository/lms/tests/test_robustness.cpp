// NFR-06: invalid input and failures must be reported as LmsError, never crash the program.
#include <functional>

#include "fixture.hpp"

using namespace lms;
using namespace lms::test;

TEST(Robustness, AtLeastFiftyInvalidInputsAreRejectedCleanly) {
  World w; w.seed();
  const User& a = *w.adminUser;
  const TimePoint future = w.clock.now() + kDay;
  const std::string longText(5000, 'x');
  std::vector<std::pair<std::string, std::function<void()>>> cases;
#define BAD(name, expr) cases.push_back({name, [&] { (void)(expr); }})
  BAD("login empty", w.auth.login("", ""));
  BAD("login whitespace", w.auth.login("   ", "   "));
  BAD("login sql text", w.auth.login("' OR 1=1 --", "x"));
  BAD("login long", w.auth.login(longText, longText));
  BAD("login nul byte", w.auth.login(std::string("a\0b", 3), "x"));
  BAD("create user empty name", w.admin.createUser(a, Role::Student, "", "N", "Passw0rd1", std::nullopt));
  BAD("create user long name", w.admin.createUser(a, Role::Student, longText, "N", "Passw0rd1", std::nullopt));
  BAD("create user symbols", w.admin.createUser(a, Role::Student, "a;b", "N", "Passw0rd1", std::nullopt));
  BAD("create user long fullname", w.admin.createUser(a, Role::Student, "okname", longText, "Passw0rd1", std::nullopt));
  BAD("create user weak pw", w.admin.createUser(a, Role::Student, "okname", "N", "1234", std::nullopt));
  BAD("create user hod no dept", w.admin.createUser(a, Role::Hod, "okname", "N", "Passw0rd1", std::nullopt));
  BAD("create user hod bad dept", w.admin.createUser(a, Role::Hod, "okname", "N", "Passw0rd1", -5));
  BAD("create user by student", w.admin.createUser(*w.stu, Role::Student, "okname", "N", "Passw0rd1", std::nullopt));
  BAD("set active missing user", w.admin.setActive(a, -1, true));
  BAD("set active by lecturer", w.admin.setActive(*w.lec, w.stu->id(), false));
  BAD("department empty", w.admin.createDepartment(a, ""));
  BAD("department long", w.admin.createDepartment(a, longText));
  BAD("course bad dept", w.admin.createCourse(a, "C1", "T", 0));
  BAD("course empty code", w.admin.createCourse(a, " ", "T", w.dept));
  BAD("course empty title", w.admin.createCourse(a, "C9", "", w.dept));
  BAD("course duplicate code", w.admin.createCourse(a, "CS101", "Other", w.dept));
  BAD("enrol missing student", w.admin.enrol(a, 0, w.course));
  BAD("enrol missing course", w.admin.enrol(a, w.stu->id(), 0));
  BAD("enrol duplicate", w.admin.enrol(a, w.stu->id(), w.course));
  BAD("enrol by lecturer", w.admin.enrol(*w.lec, w.stu2->id(), w.course));
  BAD("assign missing lecturer", w.admin.assignLecturer(a, 0, w.course));
  BAD("assign student as lecturer", w.admin.assignLecturer(a, w.stu->id(), w.course));
  BAD("backup empty", w.admin.backup(a, ""));
  BAD("backup bad folder", w.admin.backup(a, "/nonexistent_dir_xyz/backup.db"));
  BAD("assignment empty title", w.assignmentSvc.createAssignment(*w.lec, w.course, "", "d", future, 10));
  BAD("assignment mark zero", w.assignmentSvc.createAssignment(*w.lec, w.course, "T", "d", future, 0));
  BAD("assignment mark negative", w.assignmentSvc.createAssignment(*w.lec, w.course, "T", "d", future, -3));
  BAD("assignment mark huge", w.assignmentSvc.createAssignment(*w.lec, w.course, "T", "d", future, 1000000));
  BAD("assignment past deadline", w.assignmentSvc.createAssignment(*w.lec, w.course, "T", "d", 0, 10));
  BAD("assignment long description", w.assignmentSvc.createAssignment(*w.lec, w.course, "T", longText, future, 10));
  BAD("assignment missing course", w.assignmentSvc.createAssignment(*w.lec, -1, "T", "d", future, 10));
  BAD("assignment other course", w.assignmentSvc.createAssignment(*w.lec, w.course2, "T", "d", future, 10));
  BAD("extension missing assignment", w.assignmentSvc.grantExtension(*w.lec, -1, w.stu->id(), future, "reasons"));
  BAD("extension short reason", w.assignmentSvc.grantExtension(*w.lec, w.assignment, w.stu->id(), w.deadline + kDay, "no"));
  BAD("extension earlier", w.assignmentSvc.grantExtension(*w.lec, w.assignment, w.stu->id(), 1, "reasons"));
  BAD("extension missing student", w.assignmentSvc.grantExtension(*w.lec, w.assignment, 0, w.deadline + kDay, "reasons"));
  BAD("list assignments missing course", w.assignmentSvc.assignmentsFor(*w.stu, -1));
  BAD("list submissions missing", w.assignmentSvc.listSubmissions(*w.lec, 0));
  BAD("submit missing file", w.submissionSvc.submit(*w.stu, w.assignment, "/no/such/file.txt"));
  BAD("submit empty path", w.submissionSvc.submit(*w.stu, w.assignment, ""));
  BAD("submit directory", w.submissionSvc.submit(*w.stu, w.assignment, "/tmp"));
  BAD("submit missing assignment", w.submissionSvc.submit(*w.stu, 0, w.makeFile("f.txt")));
  BAD("submit by lecturer", w.submissionSvc.submit(*w.lec, w.assignment, w.makeFile("g.txt")));
  BAD("released mark by lecturer", w.submissionSvc.releasedMark(*w.lec, w.assignment));
  BAD("mark missing submission", w.marking.mark(*w.lec, 0, 5, "x"));
  BAD("remark missing submission", w.marking.remark(*w.lec, 0, 5, "reasons"));
  BAD("approve missing", w.moderation.approve(*w.hod, {0}));
  BAD("release missing", w.moderation.release(*w.hod, {-1}));
  BAD("adjust missing", w.moderation.adjustMark(*w.hod, 0, 5, "reasons"));
  BAD("correct missing", w.moderation.correctReleasedMark(*w.hod, 0, 5, "reasons"));
  BAD("review missing assignment", w.moderation.listForModeration(*w.hod, 0));
  BAD("audit by student", w.auditSvc.list(*w.stu, {}));
  BAD("change password wrong", w.auth.changePassword(*w.stu, "bad", "NewPass123"));
  BAD("change password weak", w.auth.changePassword(*w.stu, "Passw0rd1", "x"));
#undef BAD
  EXPECT_GE(cases.size(), 50u);
  for (const auto& c : cases) {
    try {
      c.second();
      ADD_FAILURE() << "was accepted: " << c.first;
    } catch (const LmsError&) {
      // expected: reported in plain language
    } catch (...) {
      ADD_FAILURE() << "wrong exception type: " << c.first;
    }
  }
}

TEST(Robustness, DatabaseFailureBecomesAnErrorNotACrash) {
  World w; w.seed();
  w.db.exec("DROP TABLE enrolment");  // simulate a damaged database
  EXPECT_THROW(w.courses.isEnrolled(1, 1), DatabaseError);
  EXPECT_THROW(w.submissionSvc.submit(*w.stu, w.assignment, w.makeFile("a.txt")), DatabaseError);
}
