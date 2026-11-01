// System tests derived from the use case specifications (Phase 2, section 4).
// Test names follow the pattern UCnn_<flow>, so each test traces to one flow of one use case.
#include <cmath>

#include "fixture.hpp"

using namespace lms;
using namespace lms::test;

// ================= UC18 Submit assignment =================
TEST(UC18, Main_OnTimeSubmission) {  // ST-01
  World w; w.seed();
  const Receipt r = w.submissionSvc.submit(*w.stu, w.assignment, w.makeFile("a.txt"));
  EXPECT_EQ(r.state, SubmissionState::Submitted);
  EXPECT_EQ(r.versionNo, 1);
}
TEST(UC18, A1_ResubmissionBeforeDeadline) {  // ST-02
  World w; w.seed();
  w.submissionSvc.submit(*w.stu, w.assignment, w.makeFile("a.txt"));
  const Receipt r = w.submissionSvc.submit(*w.stu, w.assignment, w.makeFile("b.txt"));
  EXPECT_EQ(r.versionNo, 2);
  EXPECT_EQ(w.subs.findById(r.submissionId)->versions.size(), 2u);
}
TEST(UC18, A2_LateSubmissionIsAcceptedAndFlagged) {  // ST-03
  World w; w.seed();
  w.clock.set(w.deadline + 2 * kDay);
  const Receipt r = w.submissionSvc.submit(*w.stu, w.assignment, w.makeFile("a.txt"));
  EXPECT_EQ(r.state, SubmissionState::Late);
  EXPECT_EQ(r.daysLate, 2);
}
TEST(UC18, A2_SubmissionAfterSeventyTwoHoursIsStillAccepted) {  // ST-04
  World w; w.seed();
  w.clock.set(w.deadline + 5 * kDay);
  EXPECT_EQ(w.submissionSvc.submit(*w.stu, w.assignment, w.makeFile("a.txt")).state, SubmissionState::Late);
}
TEST(UC18, E1_StudentNotEnrolled) {  // ST-05
  World w; w.seed();
  const auto s = w.addUser(Role::Student, "visitor");
  EXPECT_THROW(w.submissionSvc.submit(*s, w.assignment, w.makeFile("a.txt")), AccessDeniedError);
  EXPECT_FALSE(w.subs.findByStudentAndAssignment(s->id(), w.assignment));
}
TEST(UC18, E2_InvalidFile) {  // ST-06
  World w; w.seed();
  EXPECT_THROW(w.submissionSvc.submit(*w.stu, w.assignment, "no_such_file.txt"), ValidationError);
  EXPECT_THROW(w.submissionSvc.submit(*w.stu, w.assignment, w.makeFile("e.txt", 0)), ValidationError);
}
TEST(UC18, E3_ResubmissionOfLateWorkRefused) {  // ST-07
  World w; w.seed();
  w.clock.set(w.deadline + 10);
  w.submissionSvc.submit(*w.stu, w.assignment, w.makeFile("a.txt"));
  EXPECT_THROW(w.submissionSvc.submit(*w.stu, w.assignment, w.makeFile("b.txt")), RuleViolation);
}
TEST(UC18, E4_SubmissionAfterMarkingRefused) {  // ST-08
  World w; w.seed();
  w.submitAndMark();
  EXPECT_THROW(w.submissionSvc.submit(*w.stu, w.assignment, w.makeFile("b.txt")), RuleViolation);
}
TEST(UC18, E5_StorageErrorLeavesNoPartialVersion) {  // ST-09
  World w; w.seed();
  w.db.exec("DROP TABLE submission_version");
  EXPECT_THROW(w.submissionSvc.submit(*w.stu, w.assignment, w.makeFile("a.txt")), DatabaseError);
}

// ================= UC10 Mark submission =================
TEST(UC10, Main_MarkAfterDeadlineHiddenFromStudent) {  // ST-10
  World w; w.seed();
  const int sub = w.submitAndMark(15);
  EXPECT_EQ(w.subs.findById(sub)->state, SubmissionState::Marked);
  EXPECT_FALSE(w.submissionSvc.releasedMark(*w.stu, w.assignment));
  EXPECT_EQ(w.audit.list({}).size(), 1u);
}
TEST(UC10, Main_LatePenaltyShownAndApplied) {  // ST-11
  World w; w.seed();
  w.clock.set(w.deadline + kDay + 1);
  const Receipt r = w.submissionSvc.submit(*w.stu, w.assignment, w.makeFile("a.txt"));
  w.clock.set(w.clock.now() + 1);
  const MarkResult m = w.marking.mark(*w.lec, r.submissionId, 20, "Excellent but late");
  EXPECT_EQ(m.penalty.daysLate, 2);
  EXPECT_DOUBLE_EQ(m.penalty.penalty, 4);
  EXPECT_DOUBLE_EQ(m.penalty.finalMark, 16);
}
TEST(UC10, A1_RemarkWithReason) {  // ST-12
  World w; w.seed();
  const int sub = w.submitAndMark(10);
  w.marking.remark(*w.lec, sub, 12, "Second marker agreed");
  EXPECT_EQ(w.audit.list({}).size(), 2u);
}
TEST(UC10, E1_MarkOutsideRange) {  // ST-13
  World w; w.seed();
  const Receipt r = w.submissionSvc.submit(*w.stu, w.assignment, w.makeFile("a.txt"));
  w.clock.set(w.deadline + 60);
  EXPECT_THROW(w.marking.mark(*w.lec, r.submissionId, 21, "x"), ValidationError);
  EXPECT_THROW(w.marking.mark(*w.lec, r.submissionId, -0.5, "x"), ValidationError);
}
TEST(UC10, E2_FeedbackMissing) {  // ST-14
  World w; w.seed();
  const Receipt r = w.submissionSvc.submit(*w.stu, w.assignment, w.makeFile("a.txt"));
  w.clock.set(w.deadline + 60);
  EXPECT_THROW(w.marking.mark(*w.lec, r.submissionId, 10, ""), ValidationError);
}
TEST(UC10, E3_DeadlineNotYetPassed) {  // ST-15
  World w; w.seed();
  const Receipt r = w.submissionSvc.submit(*w.stu, w.assignment, w.makeFile("a.txt"));
  EXPECT_THROW(w.marking.mark(*w.lec, r.submissionId, 10, "early"), RuleViolation);
}
TEST(UC10, E4_ModeratedOrReleasedMarkCannotBeChangedByLecturer) {  // ST-16
  World w; w.seed();
  const int sub = w.submitAndMark();
  w.moderation.approve(*w.hod, {sub});
  EXPECT_THROW(w.marking.remark(*w.lec, sub, 5, "changed my mind"), RuleViolation);
  EXPECT_THROW(w.marking.mark(*w.lec, sub, 5, "changed my mind"), RuleViolation);
}
TEST(UC10, E5_DatabaseErrorLeavesNothingSaved) {  // ST-17
  World w; w.seed();
  const Receipt r = w.submissionSvc.submit(*w.stu, w.assignment, w.makeFile("a.txt"));
  w.clock.set(w.deadline + 60);
  w.db.exec("DROP TABLE audit_log");
  EXPECT_THROW(w.marking.mark(*w.lec, r.submissionId, 10, "x"), DatabaseError);
  EXPECT_FALSE(w.marks.find(r.submissionId));
  EXPECT_EQ(w.subs.findById(r.submissionId)->state, SubmissionState::Submitted);
}

// ================= UC14 Moderate and release marks =================
TEST(UC14, Main_ApproveThenRelease) {  // ST-18
  World w; w.seed();
  const int sub = w.submitAndMark(16);
  w.moderation.approve(*w.hod, {sub});
  EXPECT_FALSE(w.submissionSvc.releasedMark(*w.stu, w.assignment));
  w.moderation.release(*w.hod, {sub});
  EXPECT_DOUBLE_EQ(w.submissionSvc.releasedMark(*w.stu, w.assignment)->finalMark, 16);
}
TEST(UC14, A1_AdjustAMark) {  // ST-19
  World w; w.seed();
  const int sub = w.submitAndMark(16);
  w.moderation.adjustMark(*w.hod, sub, 18, "Marker was harsh on part B");
  w.moderation.approve(*w.hod, {sub});
  w.moderation.release(*w.hod, {sub});
  EXPECT_DOUBLE_EQ(w.submissionSvc.releasedMark(*w.stu, w.assignment)->finalMark, 18);
}
TEST(UC14, A2_PartialRelease) {  // ST-20
  World w; w.seed();
  const int s1 = w.submitAndMark(16);
  w.clock.set(w.deadline - 10);
  const Receipt r2 = w.submissionSvc.submit(*w.stu2, w.assignment, w.makeFile("b.txt"));
  w.clock.set(w.deadline + 100);
  w.marking.mark(*w.lec, r2.submissionId, 11, "ok");
  w.moderation.approve(*w.hod, {s1, r2.submissionId});
  EXPECT_EQ(w.moderation.release(*w.hod, {s1}).done, 1);
  EXPECT_EQ(w.subs.findById(r2.submissionId)->state, SubmissionState::Moderated);
  EXPECT_FALSE(w.submissionSvc.releasedMark(*w.stu2, w.assignment));
}
TEST(UC14, A3_CorrectAReleasedMark) {  // ST-21
  World w; w.seed();
  const int sub = w.submitAndMark(16);
  w.moderation.approve(*w.hod, {sub});
  w.moderation.release(*w.hod, {sub});
  w.moderation.correctReleasedMark(*w.hod, sub, 15, "Penalty was applied twice");
  EXPECT_DOUBLE_EQ(w.submissionSvc.releasedMark(*w.stu, w.assignment)->finalMark, 15);
}
TEST(UC14, E1_UnmarkedSubmissionIsSkipped) {  // ST-22
  World w; w.seed();
  w.submissionSvc.submit(*w.stu, w.assignment, w.makeFile("a.txt"));
  const int id = w.subs.findByStudentAndAssignment(w.stu->id(), w.assignment)->id;
  const BatchResult r = w.moderation.approve(*w.hod, {id});
  EXPECT_EQ(r.done, 0);
  EXPECT_EQ(r.skipped, 1);
}
TEST(UC14, E2_CourseOutsideDepartment) {  // ST-23
  World w; w.seed();
  w.submitAndMark();
  EXPECT_THROW(w.moderation.listForModeration(*w.hod2, w.assignment), AccessDeniedError);
}
TEST(UC14, E3_ReasonMissingOrMarkOutOfRange) {  // ST-24
  World w; w.seed();
  const int sub = w.submitAndMark();
  EXPECT_THROW(w.moderation.adjustMark(*w.hod, sub, 10, "  "), ValidationError);
  EXPECT_THROW(w.moderation.adjustMark(*w.hod, sub, 99, "valid reason"), ValidationError);
}
TEST(UC14, E4_BatchRolledBackOnDatabaseError) {  // ST-25
  World w; w.seed();
  const int sub = w.submitAndMark();
  w.db.exec("CREATE TRIGGER block_state BEFORE UPDATE ON submission BEGIN SELECT RAISE(ABORT, 'simulated failure'); END;");
  EXPECT_THROW(w.moderation.approve(*w.hod, {sub}), DatabaseError);
  w.db.exec("DROP TRIGGER block_state");
  EXPECT_EQ(w.subs.findById(sub)->state, SubmissionState::Marked);
  EXPECT_FALSE(w.marks.find(sub)->approvedAt);  // the approval column was rolled back too
}

// ================= Access control across roles (FR-18, BR-9) =================
TEST(AccessControl, EachRoleIsLimitedToItsOwnFunctions) {  // ST-26
  World w; w.seed();
  EXPECT_THROW(w.submissionSvc.submit(*w.lec, w.assignment, w.makeFile("a.txt")), AccessDeniedError);
  EXPECT_THROW(w.marking.mark(*w.stu, 1, 10, "x"), AccessDeniedError);
  EXPECT_THROW(w.moderation.approve(*w.lec, {1}), AccessDeniedError);
  EXPECT_THROW(w.moderation.release(*w.stu, {1}), AccessDeniedError);
  EXPECT_THROW(w.admin.createDepartment(*w.hod, "X"), AccessDeniedError);
  EXPECT_THROW(w.assignmentSvc.createAssignment(*w.hod, w.course, "T", "d", w.deadline, 5), AccessDeniedError);
}
TEST(AccessControl, DeactivatedUserCannotPerformActions) {  // ST-27
  World w; w.seed();
  w.users.setActive(w.stu->id(), false);
  const auto frozen = w.users.findById(w.stu->id());
  EXPECT_THROW(w.submissionSvc.submit(*frozen, w.assignment, w.makeFile("a.txt")), AccessDeniedError);
}
