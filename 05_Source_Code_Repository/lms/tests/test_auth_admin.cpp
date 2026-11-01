#include "fixture.hpp"

using namespace lms;
using namespace lms::test;

TEST(Auth, LoginSucceedsAndIsCaseInsensitiveOnUsername) {
  World w; w.seed();
  EXPECT_EQ(w.auth.login("  STU1 ", "Passw0rd1")->id(), w.stu->id());
}

TEST(Auth, WrongPasswordAndUnknownUserGiveTheSameMessage) {
  World w; w.seed();
  std::string m1, m2;
  try { w.auth.login("stu1", "wrong"); } catch (const AccessDeniedError& e) { m1 = e.what(); }
  try { w.auth.login("nobody", "wrong"); } catch (const AccessDeniedError& e) { m2 = e.what(); }
  EXPECT_FALSE(m1.empty());
  EXPECT_EQ(m1, m2);  // does not reveal which usernames exist
}

TEST(Auth, DeactivatedAccountCannotLogIn) {
  World w; w.seed();
  w.admin.setActive(*w.adminUser, w.stu->id(), false);
  EXPECT_THROW(w.auth.login("stu1", "Passw0rd1"), AccessDeniedError);
  w.admin.setActive(*w.adminUser, w.stu->id(), true);
  EXPECT_NO_THROW(w.auth.login("stu1", "Passw0rd1"));
}

TEST(Auth, AccountLocksAfterFiveFailuresAndUnlocksAfterFiveMinutes) {
  World w; w.seed();
  for (int i = 0; i < AuthService::kMaxFailedLogins; ++i) EXPECT_THROW(w.auth.login("stu1", "bad"), AccessDeniedError);
  EXPECT_THROW(w.auth.login("stu1", "Passw0rd1"), AccessDeniedError);  // correct password still refused while locked
  w.clock.set(w.clock.now() + AuthService::kLockSeconds - 1);
  EXPECT_THROW(w.auth.login("stu1", "Passw0rd1"), AccessDeniedError);
  w.clock.set(w.clock.now() + 2);
  EXPECT_NO_THROW(w.auth.login("stu1", "Passw0rd1"));
}

TEST(Auth, SuccessfulLoginResetsTheFailureCounter) {
  World w; w.seed();
  for (int i = 0; i < 4; ++i) EXPECT_THROW(w.auth.login("stu1", "bad"), AccessDeniedError);
  EXPECT_NO_THROW(w.auth.login("stu1", "Passw0rd1"));
  EXPECT_EQ(w.users.loginState(w.stu->id()).failedLogins, 0);
  for (int i = 0; i < 4; ++i) EXPECT_THROW(w.auth.login("stu1", "bad"), AccessDeniedError);
  EXPECT_NO_THROW(w.auth.login("stu1", "Passw0rd1"));  // still not locked
}

TEST(Auth, PasswordsAreStoredAsSaltedHashesOnly) {
  World w; w.seed();
  EXPECT_NE(w.stu->passwordHash().find("Passw0rd1"), 0u);
  EXPECT_EQ(w.stu->passwordHash().rfind("$argon2", 0), 0u);
  EXPECT_NE(w.stu->passwordHash(), w.stu2->passwordHash());  // same password, different salt
  auto st = w.db.prepare("SELECT COUNT(*) FROM user_account WHERE password_hash LIKE '%Passw0rd1%'");
  st.step();
  EXPECT_EQ(st.integer(0), 0);
}

TEST(Auth, ChangePasswordChecksOldPasswordAndRules) {
  World w; w.seed();
  EXPECT_THROW(w.auth.changePassword(*w.stu, "wrong", "NewPass123"), AccessDeniedError);
  EXPECT_THROW(w.auth.changePassword(*w.stu, "Passw0rd1", "short1"), ValidationError);
  EXPECT_THROW(w.auth.changePassword(*w.stu, "Passw0rd1", "onlyletters"), ValidationError);
  EXPECT_THROW(w.auth.changePassword(*w.stu, "Passw0rd1", "12345678"), ValidationError);
  w.auth.changePassword(*w.stu, "Passw0rd1", "NewPass123");
  EXPECT_THROW(w.auth.login("stu1", "Passw0rd1"), AccessDeniedError);
  EXPECT_NO_THROW(w.auth.login("stu1", "NewPass123"));
}

TEST(Admin, CreateUserValidatesEveryField) {
  World w; w.seed();
  const User& a = *w.adminUser;
  EXPECT_THROW(w.admin.createUser(a, Role::Student, "ab", "Name", "Passw0rd1", std::nullopt), ValidationError);
  EXPECT_THROW(w.admin.createUser(a, Role::Student, "bad name!", "Name", "Passw0rd1", std::nullopt), ValidationError);
  EXPECT_THROW(w.admin.createUser(a, Role::Student, std::string(33, 'a'), "Name", "Passw0rd1", std::nullopt), ValidationError);
  EXPECT_THROW(w.admin.createUser(a, Role::Student, "newuser", "   ", "Passw0rd1", std::nullopt), ValidationError);
  EXPECT_THROW(w.admin.createUser(a, Role::Student, "newuser", "Name", "short", std::nullopt), ValidationError);
  EXPECT_THROW(w.admin.createUser(a, Role::Student, "newuser1", "Name", "newuser1", std::nullopt), ValidationError);
  EXPECT_THROW(w.admin.createUser(a, Role::Student, "STU1", "Name", "Passw0rd1", std::nullopt), ValidationError);  // duplicate
  EXPECT_THROW(w.admin.createUser(*w.stu, Role::Student, "newuser", "Name", "Passw0rd1", std::nullopt), AccessDeniedError);
  const int id = w.admin.createUser(a, Role::Student, "newuser", "New User", "Passw0rd1", w.dept);  // department ignored
  EXPECT_EQ(w.users.findById(id)->role(), Role::Student);
}

TEST(Admin, HeadOfDepartmentRules) {
  World w; w.seed();
  const User& a = *w.adminUser;
  EXPECT_THROW(w.admin.createUser(a, Role::Hod, "hodx", "H X", "Passw0rd1", std::nullopt), ValidationError);
  EXPECT_THROW(w.admin.createUser(a, Role::Hod, "hodx", "H X", "Passw0rd1", 9999), ValidationError);
  EXPECT_THROW(w.admin.createUser(a, Role::Hod, "hodx", "H X", "Passw0rd1", w.dept), ValidationError);  // already has one
  const int d3 = w.admin.createDepartment(a, "Physics");
  EXPECT_NO_THROW(w.admin.createUser(a, Role::Hod, "hodx", "H X", "Passw0rd1", d3));
}

TEST(Admin, DepartmentsCoursesEnrolmentsAndAssignments) {
  World w; w.seed();
  const User& a = *w.adminUser;
  EXPECT_THROW(w.admin.createDepartment(a, "  "), ValidationError);
  EXPECT_THROW(w.admin.createDepartment(*w.lec, "X"), AccessDeniedError);
  EXPECT_THROW(w.admin.createDepartment(a, "Computing"), DatabaseError);  // duplicate name
  EXPECT_THROW(w.admin.createCourse(a, "X1", "T", 9999), NotFoundError);
  EXPECT_THROW(w.admin.createCourse(a, "", "T", w.dept), ValidationError);
  EXPECT_THROW(w.admin.enrol(a, w.stu->id(), w.course), ValidationError);  // already enrolled
  EXPECT_THROW(w.admin.enrol(a, w.lec->id(), w.course), ValidationError);  // not a student
  EXPECT_THROW(w.admin.enrol(a, w.stu->id(), 9999), NotFoundError);
  EXPECT_THROW(w.admin.assignLecturer(a, w.stu->id(), w.course), ValidationError);
  EXPECT_THROW(w.admin.assignLecturer(a, w.lec->id(), w.course), ValidationError);  // already assigned
  EXPECT_NO_THROW(w.admin.assignLecturer(a, w.lec->id(), w.course2));
  EXPECT_THROW(w.admin.setActive(a, a.id(), false), RuleViolation);
  EXPECT_THROW(w.admin.setActive(a, 9999, false), NotFoundError);
  EXPECT_EQ(w.admin.listUsers(a).size(), 7u);
  EXPECT_EQ(w.admin.listDepartments(a).size(), 2u);
  EXPECT_EQ(w.admin.listCourses(*w.stu).size(), 2u);
  EXPECT_THROW(w.admin.listUsers(*w.stu), AccessDeniedError);
  EXPECT_THROW(w.admin.backup(*w.stu, "x.db"), AccessDeniedError);
  EXPECT_THROW(w.admin.backup(a, "  "), ValidationError);
}

TEST(AssignmentSvc, CreateAssignmentValidation) {
  World w; w.seed();
  const TimePoint future = w.clock.now() + kDay;
  auto make = [&](const std::string& title, int max, TimePoint due, const User& who) {
    return w.assignmentSvc.createAssignment(who, w.course, title, "desc", due, max);
  };
  EXPECT_NO_THROW(make("Ok", 1, future, *w.lec));
  EXPECT_NO_THROW(make("Ok2", 1000, future, *w.lec));
  EXPECT_THROW(make("", 10, future, *w.lec), ValidationError);
  EXPECT_THROW(make(std::string(121, 'x'), 10, future, *w.lec), ValidationError);
  EXPECT_THROW(make("T", 0, future, *w.lec), ValidationError);
  EXPECT_THROW(make("T", 1001, future, *w.lec), ValidationError);
  EXPECT_THROW(make("T", 10, w.clock.now(), *w.lec), ValidationError);      // deadline must be in the future
  EXPECT_THROW(make("T", 10, future, *w.lec2), AccessDeniedError);          // does not teach this course
  EXPECT_THROW(make("T", 10, future, *w.stu), AccessDeniedError);
  EXPECT_THROW(w.assignmentSvc.createAssignment(*w.lec, 9999, "T", "d", future, 10), NotFoundError);
  EXPECT_THROW(w.assignmentSvc.createAssignment(*w.lec, w.course, "T", "", future, 10), ValidationError);
}

TEST(AssignmentSvc, ExtensionRules) {
  World w; w.seed();
  const TimePoint ext = w.deadline + kDay;
  EXPECT_THROW(w.assignmentSvc.grantExtension(*w.lec, w.assignment, w.stu->id(), ext, "bad"), ValidationError);       // short reason
  EXPECT_THROW(w.assignmentSvc.grantExtension(*w.lec, w.assignment, w.stu->id(), w.deadline, "illness"), ValidationError);  // not later
  EXPECT_THROW(w.assignmentSvc.grantExtension(*w.lec, w.assignment, w.stu->id(), w.deadline - 1, "illness"), ValidationError);
  EXPECT_THROW(w.assignmentSvc.grantExtension(*w.lec2, w.assignment, w.stu->id(), ext, "illness"), AccessDeniedError);
  EXPECT_THROW(w.assignmentSvc.grantExtension(*w.stu, w.assignment, w.stu->id(), ext, "illness"), AccessDeniedError);
  const auto outsider = w.addUser(Role::Student, "outsider");
  EXPECT_THROW(w.assignmentSvc.grantExtension(*w.lec, w.assignment, outsider->id(), ext, "illness"), ValidationError);
  EXPECT_THROW(w.assignmentSvc.grantExtension(*w.lec, 9999, w.stu->id(), ext, "illness"), NotFoundError);
  w.assignmentSvc.grantExtension(*w.lec, w.assignment, w.stu->id(), ext, "illness");
  w.assignmentSvc.grantExtension(*w.lec, w.assignment, w.stu->id(), ext + kDay, "more illness");  // replaces
  EXPECT_EQ(w.assignments.findExtension(w.assignment, w.stu->id())->newDeadline, ext + kDay);
}

TEST(AssignmentSvc, ListingsRespectRoles) {
  World w; w.seed();
  EXPECT_EQ(w.assignmentSvc.coursesFor(*w.stu).size(), 1u);
  EXPECT_EQ(w.assignmentSvc.coursesFor(*w.lec).size(), 1u);
  EXPECT_EQ(w.assignmentSvc.coursesFor(*w.hod).size(), 1u);
  EXPECT_EQ(w.assignmentSvc.coursesFor(*w.adminUser).size(), 2u);
  auto rows = w.assignmentSvc.assignmentsFor(*w.stu, w.course);
  ASSERT_EQ(rows.size(), 1u);
  EXPECT_FALSE(rows[0].state);
  w.submissionSvc.submit(*w.stu, w.assignment, w.makeFile("a.txt"));
  EXPECT_EQ(*w.assignmentSvc.assignmentsFor(*w.stu, w.course)[0].state, SubmissionState::Submitted);
  EXPECT_FALSE(w.assignmentSvc.assignmentsFor(*w.hod, w.course)[0].state);  // staff do not see a student's state
  EXPECT_THROW(w.assignmentSvc.assignmentsFor(*w.stu, w.course2), AccessDeniedError);
  EXPECT_THROW(w.assignmentSvc.assignmentsFor(*w.lec, w.course2), AccessDeniedError);
  EXPECT_THROW(w.assignmentSvc.assignmentsFor(*w.hod2, w.course), AccessDeniedError);
  EXPECT_EQ(w.assignmentSvc.studentsIn(*w.lec, w.course).size(), 2u);
  EXPECT_THROW(w.assignmentSvc.studentsIn(*w.stu, w.course), AccessDeniedError);
  EXPECT_EQ(w.assignmentSvc.listSubmissions(*w.lec, w.assignment).size(), 1u);
  EXPECT_THROW(w.assignmentSvc.listSubmissions(*w.lec2, w.assignment), AccessDeniedError);
  EXPECT_THROW(w.assignmentSvc.listSubmissions(*w.stu, w.assignment), AccessDeniedError);
}
