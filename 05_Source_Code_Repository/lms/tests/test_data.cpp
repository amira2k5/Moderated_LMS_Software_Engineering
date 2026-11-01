#include "fixture.hpp"

using namespace lms;
using namespace lms::test;

TEST(Data, ForeignKeysAreEnforced) {
  Database db(":memory:");
  EXPECT_THROW(db.exec("INSERT INTO enrolment (student_id, course_id) VALUES (99, 99)"), DatabaseError);
}

TEST(Data, AuditLogIsAppendOnly) {
  World w; w.seed();
  const int sub = w.submitAndMark();
  ASSERT_EQ(w.audit.list({}).size(), 1u);
  EXPECT_THROW(w.db.exec("UPDATE audit_log SET reason = 'tampered'"), DatabaseError);
  EXPECT_THROW(w.db.exec("DELETE FROM audit_log"), DatabaseError);
  EXPECT_EQ(w.audit.list({}).size(), 1u);
  (void)sub;
}

TEST(Data, ConstraintsRejectInvalidRows) {
  World w; w.seed();
  EXPECT_THROW(w.db.exec("INSERT INTO user_account (username, full_name, role, password_hash) VALUES ('h', 'H', 'HOD', 'x')"), DatabaseError);
  EXPECT_THROW(w.db.exec("INSERT INTO user_account (username, full_name, role, password_hash, department_id) VALUES ('s', 'S', 'STUDENT', 'x', 1)"), DatabaseError);
  EXPECT_THROW(w.db.exec("INSERT INTO user_account (username, full_name, role, password_hash, department_id) VALUES ('h3', 'H', 'HOD', 'x', 1)"), DatabaseError);  // second HoD
  EXPECT_THROW(w.db.exec("INSERT INTO user_account (username, full_name, role, password_hash) VALUES ('b', 'B', 'BOSS', 'x')"), DatabaseError);
  EXPECT_THROW(w.db.exec("INSERT INTO user_account (username, full_name, role, password_hash) VALUES ('STU1', 'dup', 'STUDENT', 'x')"), DatabaseError);  // case-insensitive unique
  EXPECT_THROW(w.db.exec("UPDATE assignment SET max_mark = 1001"), DatabaseError);
  EXPECT_THROW(w.db.exec("UPDATE assignment SET max_mark = 0"), DatabaseError);
  EXPECT_THROW(w.db.exec("INSERT INTO submission (assignment_id, student_id, state) VALUES (1, 5, 'WRONG')"), DatabaseError);
}

TEST(Data, QueriesAreParameterisedAgainstInjection) {
  World w;
  const std::string evil = "x'; DROP TABLE user_account; --";
  w.users.add(Role::Student, NewUser{evil, "Evil Name", "hash", std::nullopt});
  auto found = w.users.findByUsername(evil);
  ASSERT_TRUE(found);
  EXPECT_EQ(found->username(), evil);
  EXPECT_EQ(w.users.listAll().size(), 1u);  // the table still exists
  EXPECT_FALSE(w.users.findByUsername("x' OR '1'='1"));
}

TEST(Data, UnitOfWorkRollsBackAndCommitsAndNests) {
  World w;
  { auto tx = w.uow.begin(); w.courses.addDepartment("Rolled back"); }
  EXPECT_TRUE(w.courses.listDepartments().empty());
  { auto tx = w.uow.begin(); w.courses.addDepartment("Kept"); tx->commit(); }
  EXPECT_EQ(w.courses.listDepartments().size(), 1u);
  {  // an inner transaction that is not committed cancels the whole outer change
    auto outer = w.uow.begin();
    { auto inner = w.uow.begin(); w.courses.addDepartment("Inner"); }
    EXPECT_THROW(outer->commit(), DatabaseError);
  }
  EXPECT_EQ(w.courses.listDepartments().size(), 1u);
  { auto outer = w.uow.begin(); { auto inner = w.uow.begin(); w.courses.addDepartment("Both"); inner->commit(); } outer->commit(); }
  EXPECT_EQ(w.courses.listDepartments().size(), 2u);
}

TEST(Data, RepositoriesRoundTripEntities) {
  World w; w.seed();
  EXPECT_EQ(w.users.findById(w.hod->id())->role(), Role::Hod);
  EXPECT_EQ(static_cast<HeadOfDepartment&>(*w.users.findByUsername("HOD1")).departmentId(), w.dept);
  EXPECT_EQ(w.users.listAll().size(), 7u);
  EXPECT_FALSE(w.users.findById(9999));
  w.users.setActive(w.stu->id(), false);
  EXPECT_FALSE(w.users.findById(w.stu->id())->active());
  const LoginState ls{3, T("2026-11-01T09:00:00Z")};
  w.users.setLoginState(w.stu->id(), ls);
  EXPECT_EQ(w.users.loginState(w.stu->id()).failedLogins, 3);
  EXPECT_EQ(*w.users.loginState(w.stu->id()).lockedUntil, *ls.lockedUntil);
  EXPECT_EQ(w.courses.coursesForStudent(w.stu->id()).size(), 1u);
  EXPECT_EQ(w.courses.coursesForLecturer(w.lec->id()).front().code, "CS101");
  EXPECT_EQ(w.courses.coursesForDepartment(w.dept).size(), 1u);
  EXPECT_EQ(w.courses.studentsInCourse(w.course).size(), 2u);
  EXPECT_FALSE(w.courses.findCourse(9999));
  EXPECT_FALSE(w.courses.findDepartment(9999));
  EXPECT_TRUE(w.courses.isEnrolled(w.stu->id(), w.course));
  EXPECT_FALSE(w.courses.teaches(w.lec->id(), w.course2));
  EXPECT_FALSE(w.assignments.findById(9999));
  EXPECT_EQ(w.assignments.listByCourse(w.course).size(), 1u);
  EXPECT_FALSE(w.subs.findById(9999));
  EXPECT_FALSE(w.marks.find(9999));
}

TEST(Data, ExtensionIsUpsertedPerStudentAndAssignment) {
  World w; w.seed();
  Extension e; e.assignmentId = w.assignment; e.studentId = w.stu->id(); e.newDeadline = w.deadline + kDay;
  e.reason = "illness"; e.grantedBy = w.lec->id(); e.grantedAt = w.clock.now();
  const int id1 = w.assignments.saveExtension(e);
  e.newDeadline = w.deadline + 2 * kDay;
  const int id2 = w.assignments.saveExtension(e);
  EXPECT_EQ(id1, id2);
  EXPECT_EQ(w.assignments.findExtension(w.assignment, w.stu->id())->newDeadline, w.deadline + 2 * kDay);
  EXPECT_FALSE(w.assignments.findExtension(w.assignment, w.stu2->id()));
}

TEST(Data, AuditFilterByDepartmentUserAndTime) {
  World w; w.seed();
  w.submitAndMark();
  AuditFilter f; f.departmentId = w.dept;
  EXPECT_EQ(w.audit.list(f).size(), 1u);
  f.departmentId = w.dept2;
  EXPECT_TRUE(w.audit.list(f).empty());
  AuditFilter g; g.changedBy = w.lec->id(); g.from = w.clock.now() - 10; g.to = w.clock.now() + 10; g.submissionId = 1;
  EXPECT_EQ(w.audit.list(g).size(), 1u);
  g.from = w.clock.now() + 100;
  EXPECT_TRUE(w.audit.list(g).empty());
}

TEST(Data, BackupCreatesAnOpenableCopy) {
  World w; w.seed();
  const std::string dest = (w.tmp.path / "backup.db").string();
  w.admin.backup(*w.adminUser, dest);
  Database copy(dest);
  SqliteUserRepository users(copy);
  EXPECT_EQ(users.listAll().size(), 7u);
  EXPECT_THROW(w.db.backupTo((w.tmp.path / "no_such_folder" / "x.db").string()), DatabaseError);
  EXPECT_THROW(Database("/nonexistent_folder_xyz/lms.db"), DatabaseError);
}
