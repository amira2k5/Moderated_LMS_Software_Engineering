#include <cmath>

#include "fixture.hpp"

using namespace lms;
using namespace lms::test;

// ---------- Submit assignment (UC18, FR-07 to FR-09, BR-1 to BR-7) ----------
TEST(Submission, OnTimeFirstSubmission) {
  World w; w.seed();
  const Receipt r = w.submissionSvc.submit(*w.stu, w.assignment, w.makeFile("a.txt"));
  EXPECT_EQ(r.state, SubmissionState::Submitted);
  EXPECT_EQ(r.versionNo, 1);
  EXPECT_EQ(r.daysLate, 0);
  auto s = w.subs.findByStudentAndAssignment(w.stu->id(), w.assignment);
  ASSERT_TRUE(s);
  EXPECT_EQ(s->state, SubmissionState::Submitted);
  EXPECT_EQ(s->currentVersion().originalName, "a.txt");
  EXPECT_TRUE(fs::exists(s->currentVersion().storedPath));
}

TEST(Submission, DeadlineBoundaries) {
  struct Case { TimePoint offset; SubmissionState state; int days; };
  const Case cases[] = {{-1, SubmissionState::Submitted, 0}, {0, SubmissionState::Submitted, 0},
                        {1, SubmissionState::Late, 1}, {kDay, SubmissionState::Late, 1},
                        {kDay + 1, SubmissionState::Late, 2}, {3 * kDay, SubmissionState::Late, 3},
                        {3 * kDay + 1, SubmissionState::Late, 4}};  // late work is accepted, never rejected
  for (const auto& c : cases) {
    World w; w.seed();
    w.clock.set(w.deadline + c.offset);
    const Receipt r = w.submissionSvc.submit(*w.stu, w.assignment, w.makeFile("a.txt"));
    EXPECT_EQ(r.state, c.state) << "offset " << c.offset;
    EXPECT_EQ(r.daysLate, c.days) << "offset " << c.offset;
  }
}

TEST(Submission, ResubmissionBeforeDeadlineKeepsAllVersions) {
  World w; w.seed();
  const Receipt r1 = w.submissionSvc.submit(*w.stu, w.assignment, w.makeFile("v1.txt"));
  w.clock.set(w.clock.now() + 3600);
  const Receipt r2 = w.submissionSvc.submit(*w.stu, w.assignment, w.makeFile("v2.txt"));
  EXPECT_EQ(r2.submissionId, r1.submissionId);
  EXPECT_EQ(r2.versionNo, 2);
  auto s = w.subs.findById(r1.submissionId);
  ASSERT_EQ(s->versions.size(), 2u);
  EXPECT_EQ(s->currentVersion().originalName, "v2.txt");
  EXPECT_TRUE(fs::exists(s->versions[0].storedPath));  // earlier version retained (BR-6)
}

TEST(Submission, CannotResubmitAfterDeadlineOrAfterLateOrAfterMarking) {
  World w; w.seed();
  w.submissionSvc.submit(*w.stu, w.assignment, w.makeFile("a.txt"));
  w.clock.set(w.deadline + 1);
  EXPECT_THROW(w.submissionSvc.submit(*w.stu, w.assignment, w.makeFile("b.txt")), RuleViolation);
  EXPECT_EQ(w.subs.findByStudentAndAssignment(w.stu->id(), w.assignment)->versions.size(), 1u);

  World l; l.seed();
  l.clock.set(l.deadline + 5);
  l.submissionSvc.submit(*l.stu, l.assignment, l.makeFile("late.txt"));
  EXPECT_THROW(l.submissionSvc.submit(*l.stu, l.assignment, l.makeFile("again.txt")), RuleViolation);

  World m; m.seed();
  m.submitAndMark();
  EXPECT_THROW(m.submissionSvc.submit(*m.stu, m.assignment, m.makeFile("after.txt")), RuleViolation);
}

TEST(Submission, OnlyEnrolledStudentsMaySubmit) {
  World w; w.seed();
  const auto outsider = w.addUser(Role::Student, "outsider");
  EXPECT_THROW(w.submissionSvc.submit(*outsider, w.assignment, w.makeFile("a.txt")), AccessDeniedError);
  EXPECT_THROW(w.submissionSvc.submit(*w.lec, w.assignment, w.makeFile("a.txt")), AccessDeniedError);
  EXPECT_THROW(w.submissionSvc.submit(*w.stu, 9999, w.makeFile("a.txt")), NotFoundError);
}

TEST(Submission, InvalidFilesAreRejectedAndStoreNothing) {
  World w; w.seed();
  EXPECT_THROW(w.submissionSvc.submit(*w.stu, w.assignment, (w.tmp.path / "missing.txt").string()), ValidationError);
  EXPECT_THROW(w.submissionSvc.submit(*w.stu, w.assignment, w.tmp.path.string()), ValidationError);  // a folder
  EXPECT_THROW(w.submissionSvc.submit(*w.stu, w.assignment, w.makeFile("empty.txt", 0)), ValidationError);
  EXPECT_THROW(w.submissionSvc.submit(*w.stu, w.assignment, w.makeFile("big.bin", LocalFileStore::kMaxBytes + 1)), ValidationError);
  EXPECT_FALSE(w.subs.findByStudentAndAssignment(w.stu->id(), w.assignment));
  EXPECT_NO_THROW(w.submissionSvc.submit(*w.stu, w.assignment, w.makeFile("exactly5mb.bin", LocalFileStore::kMaxBytes)));
}

TEST(Submission, ExtensionMovesTheEffectiveDeadline) {
  World w; w.seed();
  w.assignmentSvc.grantExtension(*w.lec, w.assignment, w.stu->id(), w.deadline + 2 * kDay, "family emergency");
  w.clock.set(w.deadline + kDay + 12 * 3600);
  EXPECT_EQ(w.submissionSvc.submit(*w.stu, w.assignment, w.makeFile("a.txt")).state, SubmissionState::Submitted);
  EXPECT_EQ(w.submissionSvc.submit(*w.stu, w.assignment, w.makeFile("b.txt")).versionNo, 2);  // resubmission still allowed
  EXPECT_EQ(w.submissionSvc.submit(*w.stu2, w.assignment, w.makeFile("c.txt")).state, SubmissionState::Late);  // no extension
}

TEST(Submission, DatabaseFailureLeavesNoPartialSubmission) {
  World w; w.seed();
  struct FailingStore : IFileStore {
    LocalFileStore& inner; bool removed = false;
    explicit FailingStore(LocalFileStore& s) : inner(s) {}
    StoredFile store(const std::string& p) override { auto f = inner.store(p); f.sizeBytes = 0; return f; }  // violates size >= 1
    void remove(const std::string& p) noexcept override { removed = true; inner.remove(p); }
  } bad(w.store);
  SubmissionService svc(w.assignments, w.subs, w.courses, w.marks, bad, w.uow, w.clock);
  EXPECT_THROW(svc.submit(*w.stu, w.assignment, w.makeFile("a.txt")), DatabaseError);
  EXPECT_TRUE(bad.removed);  // the stored copy was cleaned up
  EXPECT_FALSE(w.subs.findByStudentAndAssignment(w.stu->id(), w.assignment));
}

// ---------- Mark submission (UC10, FR-11 to FR-13, BR-3, BR-4, BR-8, BR-10) ----------
TEST(Marking, CannotMarkBeforeTheEffectiveDeadline) {
  World w; w.seed();
  const Receipt r = w.submissionSvc.submit(*w.stu, w.assignment, w.makeFile("a.txt"));
  EXPECT_THROW(w.marking.mark(*w.lec, r.submissionId, 15, "fine"), RuleViolation);
  w.clock.set(w.deadline);  // exactly at the deadline is still not "after"
  EXPECT_THROW(w.marking.mark(*w.lec, r.submissionId, 15, "fine"), RuleViolation);
  w.clock.set(w.deadline + 1);
  EXPECT_NO_THROW(w.marking.mark(*w.lec, r.submissionId, 15, "fine"));
}

TEST(Marking, OnTimeMarkHasNoPenaltyAndWritesAnAuditEntry) {
  World w; w.seed();
  const int sub = w.submitAndMark(16);
  auto m = w.marks.find(sub);
  ASSERT_TRUE(m);
  EXPECT_DOUBLE_EQ(m->finalMark, 16);
  EXPECT_EQ(m->daysLate, 0);
  EXPECT_EQ(w.subs.findById(sub)->state, SubmissionState::Marked);
  const auto log = w.audit.list({});
  ASSERT_EQ(log.size(), 1u);
  EXPECT_EQ(log[0].action, AuditAction::InitialMark);
  EXPECT_FALSE(log[0].oldValue);
  EXPECT_DOUBLE_EQ(log[0].newValue, 16);
  EXPECT_EQ(log[0].changedBy, w.lec->id());
}

TEST(Marking, LatePenaltyIsApplied) {
  struct Case { TimePoint offset; double final; int days; };
  const Case cases[] = {{1, 14, 1}, {kDay + 1, 12, 2}, {3 * kDay, 10, 3}, {3 * kDay + 1, 0, 4}};
  for (const auto& c : cases) {
    World w; w.seed();
    w.clock.set(w.deadline + c.offset);
    const Receipt r = w.submissionSvc.submit(*w.stu, w.assignment, w.makeFile("a.txt"));
    w.clock.set(w.clock.now() + 60);
    const MarkResult m = w.marking.mark(*w.lec, r.submissionId, 16, "Marked");
    EXPECT_EQ(m.penalty.daysLate, c.days) << c.offset;
    EXPECT_DOUBLE_EQ(m.penalty.finalMark, c.final) << c.offset;
    EXPECT_DOUBLE_EQ(w.marks.find(r.submissionId)->finalMark, c.final);
    EXPECT_EQ(w.subs.findById(r.submissionId)->state, SubmissionState::Marked);
  }
}

TEST(Marking, ExtensionRemovesThePenalty) {
  World w; w.seed();
  w.assignmentSvc.grantExtension(*w.lec, w.assignment, w.stu->id(), w.deadline + 2 * kDay, "illness");
  w.clock.set(w.deadline + kDay + 12 * 3600);
  const Receipt r = w.submissionSvc.submit(*w.stu, w.assignment, w.makeFile("a.txt"));
  w.clock.set(w.deadline + 2 * kDay + 60);  // after the extended deadline
  EXPECT_DOUBLE_EQ(w.marking.mark(*w.lec, r.submissionId, 16, "ok").penalty.finalMark, 16);
}

TEST(Marking, RejectsInvalidMarksFeedbackAndWrongPeople) {
  World w; w.seed();
  const Receipt r = w.submissionSvc.submit(*w.stu, w.assignment, w.makeFile("a.txt"));
  w.clock.set(w.deadline + 60);
  EXPECT_THROW(w.marking.mark(*w.lec, r.submissionId, -1, "x"), ValidationError);
  EXPECT_THROW(w.marking.mark(*w.lec, r.submissionId, 20.01, "x"), ValidationError);
  EXPECT_THROW(w.marking.mark(*w.lec, r.submissionId, std::nan(""), "x"), ValidationError);
  EXPECT_THROW(w.marking.mark(*w.lec, r.submissionId, 10, "   "), ValidationError);
  EXPECT_THROW(w.marking.mark(*w.lec2, r.submissionId, 10, "x"), AccessDeniedError);
  EXPECT_THROW(w.marking.mark(*w.stu, r.submissionId, 10, "x"), AccessDeniedError);
  EXPECT_THROW(w.marking.mark(*w.hod, r.submissionId, 10, "x"), AccessDeniedError);
  EXPECT_THROW(w.marking.mark(*w.lec, 9999, 10, "x"), NotFoundError);
  EXPECT_EQ(w.subs.findById(r.submissionId)->state, SubmissionState::Submitted);  // nothing changed
  EXPECT_FALSE(w.marks.find(r.submissionId));
  EXPECT_NO_THROW(w.marking.mark(*w.lec, r.submissionId, 0, "zero is a valid mark"));
}

TEST(Marking, RemarkRequiresReasonAndIsAudited) {
  World w; w.seed();
  const int sub = w.submitAndMark(16);
  EXPECT_THROW(w.marking.mark(*w.lec, sub, 10, "again"), RuleViolation);  // already marked: use re-mark
  EXPECT_THROW(w.marking.remark(*w.lec, sub, 18, "oops"), ValidationError);  // reason too short
  EXPECT_THROW(w.marking.remark(*w.lec, sub, 25, "valid reason"), ValidationError);
  const MarkResult r = w.marking.remark(*w.lec, sub, 18, "Missed a page");
  EXPECT_DOUBLE_EQ(r.penalty.finalMark, 18);
  w.marking.remark(*w.lec, sub, 17, "Second look", std::string("Updated feedback"));
  EXPECT_EQ(w.marks.find(sub)->feedback, "Updated feedback");
  const auto log = w.audit.list({});
  ASSERT_EQ(log.size(), 3u);
  EXPECT_EQ(log[1].action, AuditAction::Remark);
  EXPECT_DOUBLE_EQ(*log[1].oldValue, 16);
  EXPECT_DOUBLE_EQ(log[1].newValue, 18);
  EXPECT_EQ(log[1].reason, "Missed a page");
}

TEST(Marking, FailedAuditRollsBackTheMark) {
  World w; w.seed();
  struct Thrower : IMarkChangeObserver { void onMarkChanged(const MarkChangedEvent&) override { throw DatabaseError("audit down"); } } t;
  w.events.subscribe(&t);
  const Receipt r = w.submissionSvc.submit(*w.stu, w.assignment, w.makeFile("a.txt"));
  w.clock.set(w.deadline + 60);
  EXPECT_THROW(w.marking.mark(*w.lec, r.submissionId, 15, "x"), DatabaseError);
  EXPECT_FALSE(w.marks.find(r.submissionId));  // no mark without an audit entry
  EXPECT_EQ(w.subs.findById(r.submissionId)->state, SubmissionState::Submitted);
  EXPECT_TRUE(w.audit.list({}).empty());
}
