// Moderated LMS: program entry point (wires concrete classes into the services).
#include <iostream>
#include <memory>
#include <string>
#include <unistd.h>

#include "ui.hpp"

using namespace lms;

namespace {
void bootstrapAdministrator(IUserRepository& users, const IPasswordHasher& hasher, std::istream& in, std::ostream& out) {
  out << "No accounts exist yet. Create the first administrator account.\n";
  for (;;) {
    std::string username, full, password;
    out << "Username: "; std::getline(in, username);
    out << "Full name: "; std::getline(in, full);
    out << "Password (at least 8 characters, with a letter and a digit): "; std::getline(in, password);
    try {
      username = trim(username);
      AdminService::validatePassword(password, username);
      if (username.size() < 3 || trim(full).empty()) throw ValidationError("Please enter a username of at least 3 characters and a full name.");
      users.add(Role::Admin, NewUser{username, trim(full), hasher.hash(password), std::nullopt});
      out << "Administrator account created.\n";
      return;
    } catch (const LmsError& e) {
      out << "! " << e.what() << "\n";
      if (!in) return;
    }
  }
}
}  // namespace

int main(int argc, char** argv) {
  std::string dbPath = "lms.db", filesDir = "lms_files";
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--db" && i + 1 < argc) dbPath = argv[++i];
    else if (a == "--files" && i + 1 < argc) filesDir = argv[++i];
    else if (a == "--version") { std::cout << "lms 1.1.0\n"; return 0; }
    else if (a == "--help") { std::cout << "Usage: lms [--db FILE] [--files FOLDER] [--version]\n"; return 0; }
    else { std::cerr << "Unknown option: " << a << "\nUsage: lms [--db FILE] [--files FOLDER] [--version]\n"; return 2; }
  }
  try {
    Database db(dbPath);
    SystemClock clock;
    LibsodiumHasher hasher;
    SqliteUserRepository users(db);
    SqliteCourseRepository courses(db);
    SqliteAssignmentRepository assignments(db);
    SqliteSubmissionRepository submissions(db);
    SqliteMarkRepository marks(db);
    SqliteAuditRepository audit(db);
    SqliteUnitOfWork uow(db);
    LocalFileStore files(filesDir);
    TenPercentPerDayPolicy policy;
    MarkChangePublisher events;
    AuditObserver auditObserver(audit);
    events.subscribe(&auditObserver);
    if (users.listAll().empty()) bootstrapAdministrator(users, hasher, std::cin, std::cout);

    AuthService auth(users, hasher, clock);
    AdminService admin(users, courses, hasher, db);
    AssignmentService assignmentSvc(assignments, courses, submissions, marks, users, uow, clock);
    SubmissionService submissionSvc(assignments, submissions, courses, marks, files, uow, clock);
    MarkingService marking(assignments, submissions, marks, courses, policy, events, uow, clock);
    ModerationService moderation(courses, assignments, submissions, marks, events, uow, clock);
    AuditService auditSvc(audit);
    AppServices services{auth, admin, assignmentSvc, submissionSvc, marking, moderation, auditSvc, clock};
    ConsoleApp app(services, std::cin, std::cout, isatty(STDIN_FILENO) != 0);
    app.run();
  } catch (const LmsError& e) {
    std::cerr << "The program cannot start: " << e.what() << "\n";
    return 1;
  } catch (const std::exception& e) {
    std::cerr << "The program stopped because of an unexpected problem: " << e.what() << "\n";
    return 1;
  }
  return 0;
}
