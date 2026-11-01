// Presentation layer: role-based console menus. Contains no business rules.
#pragma once
#include <functional>
#include <iosfwd>
#include <string>

#include "services.hpp"

namespace lms {

struct AppServices {
  AuthService& auth;
  AdminService& admin;
  AssignmentService& assignments;
  SubmissionService& submissions;
  MarkingService& marking;
  ModerationService& moderation;
  AuditService& audit;
  const Clock& clock;
};

class ConsoleApp {
 public:
  ConsoleApp(AppServices services, std::istream& in, std::ostream& out, bool hidePasswords = false)
      : s_(services), in_(in), out_(out), hidePasswords_(hidePasswords) {}
  void run();  // login loop; returns when the user quits or input ends
 private:
  struct EndOfInput {};
  void session(const User& user);
  void adminMenu(const User& u);
  void lecturerMenu(const User& u);
  void hodMenu(const User& u);
  void studentMenu(const User& u);
  void changePassword(const User& u);
  void showAudit(const User& u);
  void guarded(const std::function<void()>& action);
  std::string ask(const std::string& prompt);
  std::string askSecret(const std::string& prompt);
  int askInt(const std::string& prompt);
  double askNumber(const std::string& prompt);
  TimePoint askTime(const std::string& prompt);
  struct MenuItem { std::string label; std::function<void()> run; };
  void runMenu(const std::string& title, const std::vector<MenuItem>& items);
  void createUserFlow(const User& admin);
  void batchFlow(const User& hod, bool approve);
  void lecturerCourses(const User& u);
  void printCourses(const std::vector<Course>& courses);
  void printAssignments(const std::vector<AssignmentRow>& rows, bool showState);
  void printRows(const std::vector<SubmissionRow>& rows);
  int chooseCourse(const User& u);
  int chooseAssignment(const User& u, int courseId);
  AppServices s_;
  std::istream& in_;
  std::ostream& out_;
  bool hidePasswords_;
};

}  // namespace lms
