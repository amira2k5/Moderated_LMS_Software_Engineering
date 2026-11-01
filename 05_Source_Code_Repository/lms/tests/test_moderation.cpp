#include "fixture.hpp"

using namespace lms;
using namespace lms::test;

namespace {
// Second submission by stu2 in the same assignment, marked.
int markSecond(World& w) {
  w.clock.set(w.deadline - 3600);
  const Receipt r = w.submissionSvc.submit(*w.stu2, w.assignment, w.makeFile("two.txt"));
  w.clock.set(w.deadline + 3600);
  w.marking.mark(*w.lec, r.submissionId, 12, "Fair");
  return r.submissionId;
}
}  // namespace

TEST(Moderation, ReviewIsLimitedToOwnDepartment) {
  World w; w.seed();
  w.submitAndMark();
  EXPECT_EQ(w.moderation.listForModeration(*w.hod, w.assignment).size(), 1u);
  EXPECT_THROW(w.moderation.listForModeration(*w.hod2, w.assignment), AccessDeniedError);
  EXPECT_THROW(w.moderation.listForModeration(*w.lec, w.assignment), AccessDeniedError);
  EXPECT_THROW(w.moderation.listForModeration(*w.hod, 9999), NotFoundError);
}

TEST(Moderation, StudentsNeverSeeUnreleasedMarks) {
  World w; w.seed();
  const int sub = w.submitAndMark(16);
  EXPECT_FALSE(w.submissionSvc.releasedMark(*w.stu, w.assignment));      // Marked
  w.moderation.approve(*w.hod, {sub});
  EXPECT_FALSE(w.submissionSvc.releasedMark(*w.stu, w.assignment));      // Moderated
  w.moderation.release(*w.hod, {sub});
  const auto m = w.submissionSvc.releasedMark(*w.stu, w.assignment);     // Released
  ASSERT_TRUE(m);
  EXPECT_DOUBLE_EQ(m->finalMark, 16);
  EXPECT_EQ(m->feedback, "Good work");
  EXPECT_FALSE(w.submissionSvc.releasedMark(*w.stu2, w.assignment));     // someone else's mark
  EXPECT_THROW(w.submissionSvc.releasedMark(*w.lec, w.assignment), AccessDeniedError);
}

TEST(Moderation, AdjustMarkBeforeApprovalIsAuditedAndValidated) {
  World w; w.seed();
  const int sub = w.submitAndMark(16);
  EXPECT_THROW(w.moderation.adjustMark(*w.hod, sub, 15, "no"), ValidationError);
  EXPECT_THROW(w.moderation.adjustMark(*w.hod, sub, 21, "valid reason"), ValidationError);
  EXPECT_THROW(w.moderation.adjustMark(*w.hod, sub, -1, "valid reason"), ValidationError);
  EXPECT_THROW(w.moderation.adjustMark(*w.hod2, sub, 15, "valid reason"), AccessDeniedError);
  EXPECT_THROW(w.moderation.adjustMark(*w.lec, sub, 15, "valid reason"), AccessDeniedError);
  w.moderation.adjustMark(*w.hod, sub, 14, "Moderation across markers");
  EXPECT_DOUBLE_EQ(w.marks.find(sub)->finalMark, 14);
  const auto log = w.audit.list({});
  ASSERT_EQ(log.size(), 2u);
  EXPECT_EQ(log[1].action, AuditAction::ModerationAdjust);
  EXPECT_EQ(log[1].changedBy, w.hod->id());
  EXPECT_DOUBLE_EQ(*log[1].oldValue, 16);
  w.moderation.approve(*w.hod, {sub});
  EXPECT_THROW(w.moderation.adjustMark(*w.hod, sub, 13, "too late now"), RuleViolation);
}

TEST(Moderation, ApproveAndReleaseBatchesSkipWrongStates) {
  World w; w.seed();
  const int s1 = w.submitAndMark(16);
  const int s2 = markSecond(w);
  w.clock.set(w.deadline - 3600);
  const auto third = w.addUser(Role::Student, "stu3");
  w.admin.enrol(*w.adminUser, third->id(), w.course);
  const Receipt r3 = w.submissionSvc.submit(*third, w.assignment, w.makeFile("three.txt"));  // never marked
  const BatchResult a = w.moderation.approve(*w.hod, {s1, s2, r3.submissionId});
  EXPECT_EQ(a.done, 2);
  EXPECT_EQ(a.skipped, 1);
  EXPECT_EQ(w.subs.findById(s1)->state, SubmissionState::Moderated);
  EXPECT_TRUE(w.marks.find(s1)->approvedAt);
  EXPECT_EQ(*w.marks.find(s1)->approvedBy, w.hod->id());
  const BatchResult r = w.moderation.release(*w.hod, {s1, r3.submissionId});
  EXPECT_EQ(r.done, 1);
  EXPECT_EQ(r.skipped, 1);
  EXPECT_EQ(w.subs.findById(s1)->state, SubmissionState::Released);
  EXPECT_EQ(w.subs.findById(s2)->state, SubmissionState::Moderated);  // partial release keeps the rest
  EXPECT_EQ(w.moderation.release(*w.hod, {s1}).skipped, 1);           // already released
}

TEST(Moderation, ReleaseRequiresApprovalFirst) {
  World w; w.seed();
  const int sub = w.submitAndMark();
  const BatchResult r = w.moderation.release(*w.hod, {sub});  // still only Marked
  EXPECT_EQ(r.done, 0);
  EXPECT_EQ(r.skipped, 1);
  EXPECT_EQ(w.subs.findById(sub)->state, SubmissionState::Marked);
}

TEST(Moderation, BatchIsAllOrNothing) {
  World w; w.seed();
  const int own = w.submitAndMark();
  // A marked submission in the other department.
  const int other = [&] {
    const TimePoint due = w.clock.now() + kDay;
    const int a2 = w.assignmentSvc.createAssignment(*w.lec2, w.course2, "Maths 1", "d", due, 10);
    const auto s = w.addUser(Role::Student, "mathstu");
    w.admin.enrol(*w.adminUser, s->id(), w.course2);
    const Receipt r = w.submissionSvc.submit(*s, a2, w.makeFile("m.txt"));
    w.clock.set(due + 60);
    w.marking.mark(*w.lec2, r.submissionId, 8, "ok");
    return r.submissionId;
  }();
  EXPECT_THROW(w.moderation.approve(*w.hod, {own, other}), AccessDeniedError);
  EXPECT_EQ(w.subs.findById(own)->state, SubmissionState::Marked);  // not half-applied
  EXPECT_NO_THROW(w.moderation.approve(*w.hod2, {other}));
}

TEST(Moderation, CorrectionOfAReleasedMarkIsAuditedAndVisible) {
  World w; w.seed();
  const int sub = w.submitAndMark(16);
  EXPECT_THROW(w.moderation.correctReleasedMark(*w.hod, sub, 17, "not released yet"), RuleViolation);
  w.moderation.approve(*w.hod, {sub});
  w.moderation.release(*w.hod, {sub});
  EXPECT_THROW(w.moderation.correctReleasedMark(*w.hod, sub, 17, "no"), ValidationError);
  w.moderation.correctReleasedMark(*w.hod, sub, 17, "Addition error found");
  EXPECT_DOUBLE_EQ(w.submissionSvc.releasedMark(*w.stu, w.assignment)->finalMark, 17);
  const auto log = w.audit.list({});
  EXPECT_EQ(log.back().action, AuditAction::ReleasedCorrection);
  EXPECT_DOUBLE_EQ(*log.back().oldValue, 16);
}

TEST(Moderation, AuditLogAccessIsRoleBased) {
  World w; w.seed();
  w.submitAndMark();
  EXPECT_EQ(w.auditSvc.list(*w.adminUser, {}).size(), 1u);
  EXPECT_EQ(w.auditSvc.list(*w.hod, {}).size(), 1u);
  EXPECT_TRUE(w.auditSvc.list(*w.hod2, {}).empty());  // a HoD sees only their own department
  AuditFilter wide; wide.departmentId = w.dept;       // a HoD cannot widen the filter
  EXPECT_TRUE(w.auditSvc.list(*w.hod2, wide).empty());
  EXPECT_THROW(w.auditSvc.list(*w.lec, {}), AccessDeniedError);
  EXPECT_THROW(w.auditSvc.list(*w.stu, {}), AccessDeniedError);
}
