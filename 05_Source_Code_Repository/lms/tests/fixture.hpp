// Shared test fixture: an in-memory database, a fixed clock and every service wired together.
#pragma once
#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>
#include <fstream>

#include "ui.hpp"

namespace lms::test {

namespace fs = std::filesystem;

inline TimePoint T(const char* iso) { return fromIso(iso); }

struct TempDir {
  fs::path path = fs::temp_directory_path() / ("lmstest_" + std::to_string(::getpid()) + "_" + std::to_string(next()));
  TempDir() { fs::create_directories(path); }
  static int next() { static std::atomic<int> n{0}; return n++; }
  ~TempDir() { std::error_code ec; fs::remove_all(path, ec); }
};

struct World {
  TempDir tmp;
  Database db{":memory:"};
  FixedClock clock{fromIso("2026-11-01T08:00:00Z")};
  LibsodiumHasher hasher{LibsodiumHasher::Strength::Fast};
  SqliteUserRepository users{db};
  SqliteCourseRepository courses{db};
  SqliteAssignmentRepository assignments{db};
  SqliteSubmissionRepository subs{db};
  SqliteMarkRepository marks{db};
  SqliteAuditRepository audit{db};
  SqliteUnitOfWork uow{db};
  LocalFileStore store;
  TenPercentPerDayPolicy policy;
  MarkChangePublisher events;
  AuditObserver auditObserver{audit};
  AuthService auth;
  AdminService admin;
  AssignmentService assignmentSvc;
  SubmissionService submissionSvc;
  MarkingService marking;
  ModerationService moderation;
  AuditService auditSvc;

  // Standard scenario (see seed()).
  std::unique_ptr<User> adminUser, hod, hod2, lec, lec2, stu, stu2;
  int dept = 0, dept2 = 0, course = 0, course2 = 0, assignment = 0;
  TimePoint deadline = fromIso("2026-11-03T08:00:00Z");  // two days after the fixed "now"

  World()
      : store((tmp.path / "files").string()),
        auth(users, hasher, clock),
        admin(users, courses, hasher, db),
        assignmentSvc(assignments, courses, subs, marks, users, uow, clock),
        submissionSvc(assignments, subs, courses, marks, store, uow, clock),
        marking(assignments, subs, marks, courses, policy, events, uow, clock),
        moderation(courses, assignments, subs, marks, events, uow, clock),
        auditSvc(audit) {
    events.subscribe(&auditObserver);
  }

  std::unique_ptr<User> addUser(Role r, const std::string& name, std::optional<int> department = std::nullopt,
                                const std::string& pw = "Passw0rd1") {
    const int id = users.add(r, NewUser{name, name + " Full", hasher.hash(pw), department});
    return users.findById(id);
  }

  std::string makeFile(const std::string& name, std::size_t bytes = 100) const {
    const fs::path p = tmp.path / name;
    std::ofstream(p, std::ios::binary) << std::string(bytes, 'x');
    return p.string();
  }

  // Department "Computing" with head hod, lecturer lec teaching CS101, students stu and stu2 enrolled,
  // and a 20-mark assignment due two days after the fixed clock time.
  void seed() {
    adminUser = addUser(Role::Admin, "admin");
    dept = courses.addDepartment("Computing");
    dept2 = courses.addDepartment("Mathematics");
    hod = addUser(Role::Hod, "hod1", dept);
    hod2 = addUser(Role::Hod, "hod2", dept2);
    lec = addUser(Role::Lecturer, "lec1");
    lec2 = addUser(Role::Lecturer, "lec2");
    stu = addUser(Role::Student, "stu1");
    stu2 = addUser(Role::Student, "stu2");
    course = admin.createCourse(*adminUser, "CS101", "Introduction", dept);
    course2 = admin.createCourse(*adminUser, "MA101", "Calculus", dept2);
    admin.assignLecturer(*adminUser, lec->id(), course);
    admin.assignLecturer(*adminUser, lec2->id(), course2);
    admin.enrol(*adminUser, stu->id(), course);
    admin.enrol(*adminUser, stu2->id(), course);
    assignment = assignmentSvc.createAssignment(*lec, course, "Assignment 1", "Solve the problems", deadline, 20);
  }

  // Student stu submits on time, the clock moves past the deadline, lecturer marks raw 16.
  int submitAndMark(double raw = 16, const User* student = nullptr) {
    const User& s = student ? *student : *stu;
    const Receipt r = submissionSvc.submit(s, assignment, makeFile("work_" + s.username() + ".txt"));
    clock.set(deadline + 3600);
    marking.mark(*lec, r.submissionId, raw, "Good work");
    return r.submissionId;
  }
};

}  // namespace lms::test
