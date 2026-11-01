#include <algorithm>
#include <iterator>

#include "fixture.hpp"

using namespace lms;
using namespace lms::test;

TEST(Penalty, BoundaryValuesForDeadlineAndDays) {
  const TenPercentPerDayPolicy p;
  const TimePoint d = T("2026-11-03T08:00:00Z");
  struct Case { TimePoint at; int days; double penalty; double final; };
  const Case cases[] = {
      {d - 1, 0, 0, 16},          {d, 0, 0, 16},                     {d + 1, 1, 2, 14},
      {d + kDay, 1, 2, 14},       {d + kDay + 1, 2, 4, 12},          {d + 2 * kDay, 2, 4, 12},
      {d + 3 * kDay, 3, 6, 10},   {d + 3 * kDay + 1, 4, 16, 0},      {d + 30 * kDay, 30, 16, 0}};
  for (const auto& c : cases) {
    const auto r = p.calculate({20, 16, d, c.at});
    EXPECT_EQ(r.daysLate, c.days) << "at offset " << (c.at - d);
    EXPECT_DOUBLE_EQ(r.penalty, c.penalty) << "at offset " << (c.at - d);
    EXPECT_DOUBLE_EQ(r.finalMark, c.final) << "at offset " << (c.at - d);
  }
}

TEST(Penalty, MarkBoundariesAndFloorAtZero) {
  const TenPercentPerDayPolicy p;
  const TimePoint d = 1000000;
  EXPECT_DOUBLE_EQ(p.calculate({20, 0, d, d + 1}).finalMark, 0);     // lowest mark stays zero
  EXPECT_DOUBLE_EQ(p.calculate({20, 20, d, d}).finalMark, 20);       // highest mark on time
  EXPECT_DOUBLE_EQ(p.calculate({20, 5, d, d + 3 * kDay}).finalMark, 0);  // penalty (6) larger than mark (5)
  EXPECT_DOUBLE_EQ(p.calculate({20, 5, d, d + 3 * kDay}).penalty, 5);
}

TEST(Penalty, RoundsHalfUpToTwoDecimals) {
  const TenPercentPerDayPolicy p;
  EXPECT_DOUBLE_EQ(p.calculate({7, 5, 0, 1}).finalMark, 4.3);
  EXPECT_DOUBLE_EQ(p.calculate({7, 5, 0, kDay + 1}).finalMark, 3.6);
  EXPECT_DOUBLE_EQ(roundHalfUp2(2.675), 2.68);
  EXPECT_DOUBLE_EQ(roundHalfUp2(1.004), 1.0);
}

TEST(Penalty, PolicyIsReplaceable) {
  const TenPercentPerDayPolicy strict(25, 2);
  EXPECT_DOUBLE_EQ(strict.calculate({100, 80, 0, 1}).finalMark, 55);
  EXPECT_DOUBLE_EQ(strict.calculate({100, 80, 0, 2 * kDay + 1}).finalMark, 0);
}

TEST(Domain, PermissionMatrixPerRole) {
  const Student s(1, "s", "S", "h", true);
  const Lecturer l(2, "l", "L", "h", true);
  const HeadOfDepartment h(3, "h", "H", "h", true, 1);
  const Administrator a(4, "a", "A", "h", true);
  const Action all[] = {Action::ManageAccounts, Action::ManageCourses, Action::EnrolAndAssign, Action::ViewCourses,
      Action::CreateAssignment, Action::GrantExtension, Action::SubmitWork, Action::MarkSubmission, Action::ReviewDepartment,
      Action::ModerateMarks, Action::ReleaseMarks, Action::ViewReleasedMarks, Action::ViewAuditLog, Action::BackupDatabase,
      Action::ChangeOwnPassword};
  auto count = [&](const User& u) { return static_cast<int>(std::count_if(std::begin(all), std::end(all), [&](Action x) { return u.allows(x); })); };
  EXPECT_EQ(count(s), 4);
  EXPECT_EQ(count(l), 5);
  EXPECT_EQ(count(h), 6);
  EXPECT_EQ(count(a), 7);
  EXPECT_TRUE(s.allows(Action::SubmitWork));      EXPECT_FALSE(s.allows(Action::MarkSubmission));
  EXPECT_TRUE(l.allows(Action::MarkSubmission));  EXPECT_FALSE(l.allows(Action::ReleaseMarks));
  EXPECT_TRUE(h.allows(Action::ReleaseMarks));    EXPECT_FALSE(h.allows(Action::MarkSubmission));
  EXPECT_TRUE(a.allows(Action::BackupDatabase));  EXPECT_FALSE(a.allows(Action::SubmitWork));
  EXPECT_EQ(s.role(), Role::Student); EXPECT_EQ(l.role(), Role::Lecturer);
  EXPECT_EQ(h.role(), Role::Hod);     EXPECT_EQ(a.role(), Role::Admin);
}

TEST(Domain, UserFactoryBuildsEachRoleAndValidates) {
  NewUser d{"u", "U User", "hash", std::nullopt};
  EXPECT_EQ(UserFactory::create(Role::Student, 1, d)->role(), Role::Student);
  EXPECT_EQ(UserFactory::create(Role::Lecturer, 2, d)->role(), Role::Lecturer);
  EXPECT_EQ(UserFactory::create(Role::Admin, 3, d)->role(), Role::Admin);
  EXPECT_THROW(UserFactory::create(Role::Hod, 4, d), ValidationError);
  d.departmentId = 7;
  auto h = UserFactory::create(Role::Hod, 4, d, false);
  EXPECT_EQ(static_cast<HeadOfDepartment&>(*h).departmentId(), 7);
  EXPECT_FALSE(h->active());
}

TEST(Domain, EnumerationsRoundTripThroughText) {
  for (auto r : {Role::Student, Role::Lecturer, Role::Hod, Role::Admin}) EXPECT_EQ(roleFromString(toString(r)), r);
  for (auto s : {SubmissionState::Submitted, SubmissionState::Late, SubmissionState::Marked, SubmissionState::Moderated,
                 SubmissionState::Released}) EXPECT_EQ(stateFromString(toString(s)), s);
  for (auto a : {AuditAction::InitialMark, AuditAction::Remark, AuditAction::ModerationAdjust, AuditAction::ReleasedCorrection})
    EXPECT_EQ(auditActionFromString(toString(a)), a);
  EXPECT_THROW(roleFromString("BOSS"), ValidationError);
  EXPECT_THROW(stateFromString("x"), ValidationError);
  EXPECT_THROW(auditActionFromString("x"), ValidationError);
  EXPECT_EQ(displayName(Role::Hod), "Head of Department");
}

TEST(Domain, EffectiveDeadlineAndCurrentVersion) {
  Assignment a; a.deadline = 100;
  EXPECT_EQ(effectiveDeadline(a, std::nullopt), 100);
  Extension e; e.newDeadline = 500;
  EXPECT_EQ(effectiveDeadline(a, e), 500);
  Submission s;
  EXPECT_THROW(s.currentVersion(), NotFoundError);
  SubmissionVersion v1; v1.versionNo = 1; SubmissionVersion v3; v3.versionNo = 3; SubmissionVersion v2; v2.versionNo = 2;
  s.versions = {v1, v3, v2};
  EXPECT_EQ(s.currentVersion().versionNo, 3);
}

TEST(Common, TimeConversionAndTrim) {
  EXPECT_EQ(toIso(fromIso("2026-11-03T08:00:00Z")), "2026-11-03T08:00:00Z");
  EXPECT_THROW(fromIso("tomorrow"), ValidationError);
  EXPECT_THROW(fromIso("2026-11-03 08:00:00"), ValidationError);
  EXPECT_THROW(parseLocal("2026-13-01 10:00"), ValidationError);
  EXPECT_THROW(parseLocal("2026-11-01 25:00"), ValidationError);
  EXPECT_THROW(parseLocal("hello"), ValidationError);
  const TimePoint t = parseLocal("2026-11-20 17:30");
  EXPECT_EQ(formatLocal(t), "2026-11-20 17:30");
  EXPECT_EQ(trim("  a b \n"), "a b");
  EXPECT_EQ(trim("   "), "");
  SystemClock c;
  EXPECT_GT(c.now(), T("2026-01-01T00:00:00Z"));
  MarkChangePublisher pub;  // no observers: publishing is harmless
  pub.publish(MarkChangedEvent{});
}
