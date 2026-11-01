// CR1: a lecturer can extend the deadline for a whole class, and extensions are shown on the student's list.
#include <sstream>

#include "fixture.hpp"

using namespace lms;
using namespace lms::test;

TEST(CR1, ClassExtensionGivesEveryEnrolledStudentTheNewDeadline) {
  World w; w.seed();
  const TimePoint ext = w.deadline + 2 * kDay;
  const ClassExtensionResult r = w.assignmentSvc.grantClassExtension(*w.lec, w.assignment, ext, "Network outage in the lab");
  EXPECT_EQ(r.granted, 2);
  EXPECT_EQ(r.kept, 0);
  EXPECT_EQ(w.assignments.findExtension(w.assignment, w.stu->id())->newDeadline, ext);
  EXPECT_EQ(w.assignments.findExtension(w.assignment, w.stu2->id())->newDeadline, ext);
  w.clock.set(w.deadline + kDay);  // after the original deadline, before the class deadline
  EXPECT_EQ(w.submissionSvc.submit(*w.stu, w.assignment, w.makeFile("a.txt")).state, SubmissionState::Submitted);
  EXPECT_EQ(w.submissionSvc.submit(*w.stu2, w.assignment, w.makeFile("b.txt")).state, SubmissionState::Submitted);
}

TEST(CR1, NeverShortensALaterIndividualExtension) {
  World w; w.seed();
  w.assignmentSvc.grantExtension(*w.lec, w.assignment, w.stu->id(), w.deadline + 5 * kDay, "Hospital stay");
  const ClassExtensionResult r =
      w.assignmentSvc.grantClassExtension(*w.lec, w.assignment, w.deadline + kDay, "Lab was closed");
  EXPECT_EQ(r.granted, 1);
  EXPECT_EQ(r.kept, 1);
  EXPECT_EQ(w.assignments.findExtension(w.assignment, w.stu->id())->newDeadline, w.deadline + 5 * kDay);
  EXPECT_EQ(w.assignments.findExtension(w.assignment, w.stu2->id())->newDeadline, w.deadline + kDay);
}

TEST(CR1, ExtendsAnEarlierIndividualExtension) {
  World w; w.seed();
  w.assignmentSvc.grantExtension(*w.lec, w.assignment, w.stu->id(), w.deadline + kDay, "Illness");
  const ClassExtensionResult r = w.assignmentSvc.grantClassExtension(*w.lec, w.assignment, w.deadline + 3 * kDay, "Power cut");
  EXPECT_EQ(r.granted, 2);
  EXPECT_EQ(w.assignments.findExtension(w.assignment, w.stu->id())->newDeadline, w.deadline + 3 * kDay);
}

TEST(CR1, ClassExtensionRemovesThePenaltyWhenMarking) {
  World w; w.seed();
  w.assignmentSvc.grantClassExtension(*w.lec, w.assignment, w.deadline + 2 * kDay, "Exam clash");
  w.clock.set(w.deadline + kDay + 100);
  const Receipt r = w.submissionSvc.submit(*w.stu, w.assignment, w.makeFile("a.txt"));
  w.clock.set(w.deadline + 2 * kDay + 100);
  EXPECT_DOUBLE_EQ(w.marking.mark(*w.lec, r.submissionId, 18, "Good").penalty.finalMark, 18);
}

TEST(CR1, ValidationAndAccessRules) {
  World w; w.seed();
  const TimePoint ext = w.deadline + kDay;
  EXPECT_THROW(w.assignmentSvc.grantClassExtension(*w.lec, w.assignment, ext, "no"), ValidationError);
  EXPECT_THROW(w.assignmentSvc.grantClassExtension(*w.lec, w.assignment, w.deadline, "valid reason"), ValidationError);
  EXPECT_THROW(w.assignmentSvc.grantClassExtension(*w.lec, w.assignment, w.deadline - 1, "valid reason"), ValidationError);
  EXPECT_THROW(w.assignmentSvc.grantClassExtension(*w.lec, 9999, ext, "valid reason"), NotFoundError);
  EXPECT_THROW(w.assignmentSvc.grantClassExtension(*w.lec2, w.assignment, ext, "valid reason"), AccessDeniedError);
  EXPECT_THROW(w.assignmentSvc.grantClassExtension(*w.stu, w.assignment, ext, "valid reason"), AccessDeniedError);
  EXPECT_THROW(w.assignmentSvc.grantClassExtension(*w.hod, w.assignment, ext, "valid reason"), AccessDeniedError);
  // A course with no students enrolled.
  const int emptyCourse = w.admin.createCourse(*w.adminUser, "CS999", "Empty", w.dept);
  w.admin.assignLecturer(*w.adminUser, w.lec->id(), emptyCourse);
  const int a2 = w.assignmentSvc.createAssignment(*w.lec, emptyCourse, "Nobody", "d", w.deadline, 10);
  EXPECT_THROW(w.assignmentSvc.grantClassExtension(*w.lec, a2, ext, "valid reason"), ValidationError);
  EXPECT_FALSE(w.assignments.findExtension(w.assignment, w.stu->id()));  // nothing changed by the failed attempts
}

TEST(CR1, ClassExtensionIsAllOrNothing) {
  World w; w.seed();
  w.db.exec("CREATE TRIGGER block_second BEFORE INSERT ON extension WHEN NEW.student_id = " +
            std::to_string(w.stu2->id()) + " BEGIN SELECT RAISE(ABORT, 'simulated failure'); END;");
  EXPECT_THROW(w.assignmentSvc.grantClassExtension(*w.lec, w.assignment, w.deadline + kDay, "Lab closed"), DatabaseError);
  EXPECT_FALSE(w.assignments.findExtension(w.assignment, w.stu->id()));  // the first student's row was rolled back
  EXPECT_FALSE(w.assignments.findExtension(w.assignment, w.stu2->id()));
}

TEST(CR1, StudentsSeeTheirExtensionOnTheAssignmentList) {
  World w; w.seed();
  w.assignmentSvc.grantExtension(*w.lec, w.assignment, w.stu->id(), w.deadline + kDay, "Illness");
  const auto mine = w.assignmentSvc.assignmentsFor(*w.stu, w.course);
  ASSERT_EQ(mine.size(), 1u);
  ASSERT_TRUE(mine[0].extendedTo);
  EXPECT_EQ(*mine[0].extendedTo, w.deadline + kDay);
  EXPECT_FALSE(w.assignmentSvc.assignmentsFor(*w.stu2, w.course)[0].extendedTo);   // no extension
  EXPECT_FALSE(w.assignmentSvc.assignmentsFor(*w.lec, w.course)[0].extendedTo);    // staff lists are unchanged
}

namespace {
std::string runScript(World& w, const std::string& script) {
  AppServices svc{w.auth, w.admin, w.assignmentSvc, w.submissionSvc, w.marking, w.moderation, w.auditSvc, w.clock};
  std::istringstream in(script);
  std::ostringstream out;
  ConsoleApp app(svc, in, out);
  app.run();
  return out.str();
}
}  // namespace

TEST(CR1, ConsoleLecturerGrantsAClassExtensionAndStudentsSeeIt) {
  World w; w.seed();
  const TimePoint ext = w.deadline + 2 * kDay;
  std::string out = runScript(w, "lec1\nPassw0rd1\n7\n" + std::to_string(w.course) + "\n" + std::to_string(w.assignment) +
                                     "\n" + formatLocal(ext) + "\nLab was closed\n0\n\n");
  EXPECT_NE(out.find("Extension granted to 2 student(s)."), std::string::npos);
  out = runScript(w, "stu2\nPassw0rd1\n1\n0\n\n");
  EXPECT_NE(out.find("(extended to " + formatLocal(ext) + ")"), std::string::npos);
}

TEST(CR1, ConsoleShowsNoExtensionTextWhenThereIsNone) {
  World w; w.seed();
  const std::string out = runScript(w, "stu1\nPassw0rd1\n1\n0\n\n");
  EXPECT_EQ(out.find("extended to"), std::string::npos);
}

TEST(CR1, ConsoleReportsStudentsWhoKeepALaterExtension) {
  World w; w.seed();
  w.assignmentSvc.grantExtension(*w.lec, w.assignment, w.stu->id(), w.deadline + 9 * kDay, "Long illness");
  const std::string out = runScript(w, "lec1\nPassw0rd1\n7\n" + std::to_string(w.course) + "\n" + std::to_string(w.assignment) +
                                           "\n" + formatLocal(w.deadline + kDay) + "\nCourse closed\n0\n\n");
  EXPECT_NE(out.find("Extension granted to 1 student(s). 1 student(s) already had a later extension and keep it."), std::string::npos);
}
