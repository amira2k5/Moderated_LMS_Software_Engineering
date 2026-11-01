#include <sstream>

#include "fixture.hpp"

using namespace lms;
using namespace lms::test;

namespace {
std::string runScript(World& w, const std::string& script) {
  AppServices svc{w.auth, w.admin, w.assignmentSvc, w.submissionSvc, w.marking, w.moderation, w.auditSvc, w.clock};
  std::istringstream in(script);
  std::ostringstream out;
  ConsoleApp app(svc, in, out);
  app.run();
  return out.str();
}
bool has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }
}  // namespace

TEST(Console, FailedLoginShowsAPlainMessageAndQuitWorks) {
  World w; w.seed();
  const std::string out = runScript(w, "stu1\nwrong\n\n");
  EXPECT_TRUE(has(out, "The username or password is not correct."));
}

TEST(Console, StudentSubmitsWork) {
  World w; w.seed();
  const std::string file = w.makeFile("essay.txt");
  const std::string out = runScript(w, "stu1\nPassw0rd1\n1\n2\n" + std::to_string(w.course) + "\n" +
                                       std::to_string(w.assignment) + "\n" + file + "\n1\n0\n\n");
  EXPECT_TRUE(has(out, "Submitted. Version 1"));
  EXPECT_TRUE(has(out, "status: SUBMITTED"));
  EXPECT_TRUE(has(out, "You are logged out."));
}

TEST(Console, LateSubmissionWarnsAboutPenalty) {
  World w; w.seed();
  w.clock.set(w.deadline + 3600);
  const std::string out = runScript(w, "stu1\nPassw0rd1\n2\n" + std::to_string(w.course) + "\n" +
                                       std::to_string(w.assignment) + "\n" + w.makeFile("e.txt") + "\n0\n\n");
  EXPECT_TRUE(has(out, "late by 1 day(s); a penalty applies"));
}

TEST(Console, LecturerMarksAndHodModeratesAndReleases) {
  World w; w.seed();
  const Receipt r = w.submissionSvc.submit(*w.stu, w.assignment, w.makeFile("a.txt"));
  w.clock.set(w.deadline + 3600);
  std::string out = runScript(w, "lec1\nPassw0rd1\n5\n" + std::to_string(r.submissionId) + "\n16\nWell argued\n0\n\n");
  EXPECT_TRUE(has(out, "final mark 16.00"));
  EXPECT_TRUE(has(out, "cannot see it until the head of department releases it"));
  EXPECT_EQ(w.subs.findById(r.submissionId)->state, SubmissionState::Marked);
  out = runScript(w, "hod1\nPassw0rd1\n4\n" + std::to_string(w.course) + "\n" + std::to_string(w.assignment) + "\nALL\n" +
                     "5\n" + std::to_string(w.course) + "\n" + std::to_string(w.assignment) + "\nALL\n0\n\n");
  EXPECT_TRUE(has(out, "Approved 1 mark(s)"));
  EXPECT_TRUE(has(out, "Released 1 mark(s)"));
  out = runScript(w, "stu1\nPassw0rd1\n3\n" + std::to_string(w.course) + "\n" + std::to_string(w.assignment) + "\n0\n\n");
  EXPECT_TRUE(has(out, "Final mark: 16.00"));
  EXPECT_TRUE(has(out, "Feedback: Well argued"));
}

TEST(Console, StudentSeesNothingBeforeRelease) {
  World w; w.seed();
  w.submitAndMark();
  const std::string out = runScript(w, "stu1\nPassw0rd1\n3\n" + std::to_string(w.course) + "\n" + std::to_string(w.assignment) + "\n0\n\n");
  EXPECT_TRUE(has(out, "No mark has been released"));
  EXPECT_FALSE(has(out, "Final mark"));
}

TEST(Console, AdministratorCreatesAnAccount) {
  World w; w.seed();
  const std::string out = runScript(w, "admin\nPassw0rd1\n1\n1\nnewstu\nNew Student\nPassw0rd9\n2\n0\n\n");
  EXPECT_TRUE(has(out, "Account created"));
  EXPECT_TRUE(has(out, "newstu"));
  EXPECT_TRUE(w.users.findByUsername("newstu"));
}

TEST(Console, InvalidInputIsExplainedAndDoesNotCrash) {
  World w; w.seed();
  const std::string out = runScript(w, "stu1\nPassw0rd1\nabc\n99\n2\nxyz\n2\n1\nabc\n0\n\n");
  EXPECT_TRUE(has(out, "Please enter a whole number."));
  EXPECT_TRUE(has(out, "Please choose a number from the list.") || has(out, "Choose"));
}

TEST(Console, EndOfInputInsideASessionEndsCleanly) {
  World w; w.seed();
  EXPECT_NO_THROW(runScript(w, "stu1\nPassw0rd1\n"));
  EXPECT_NO_THROW(runScript(w, "stu1\n"));
  EXPECT_NO_THROW(runScript(w, ""));
}

TEST(Console, ChangePasswordAndAuditLogScreens) {
  World w; w.seed();
  w.submitAndMark();
  std::string out = runScript(w, "stu1\nPassw0rd1\n4\nPassw0rd1\nNewPass123\n0\n\n");
  EXPECT_TRUE(has(out, "Your password was changed."));
  out = runScript(w, "hod1\nPassw0rd1\n7\n0\n\n");
  EXPECT_TRUE(has(out, "INITIAL_MARK"));
}
