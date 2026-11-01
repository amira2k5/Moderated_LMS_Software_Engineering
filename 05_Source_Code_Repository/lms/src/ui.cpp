#include "ui.hpp"

#include <iomanip>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace lms {

namespace {
std::string money(double v) {
  std::ostringstream o;
  o << std::fixed << std::setprecision(2) << v;
  return o.str();
}
}  // namespace

// ---------------- input helpers ----------------
std::string ConsoleApp::ask(const std::string& prompt) {
  out_ << prompt << ": " << std::flush;
  std::string line;
  if (!std::getline(in_, line)) throw EndOfInput{};
  return trim(line);
}

std::string ConsoleApp::askSecret(const std::string& prompt) {
  if (!hidePasswords_) return ask(prompt);
#ifdef _WIN32
  HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
  DWORD mode = 0;
  GetConsoleMode(h, &mode);
  SetConsoleMode(h, mode & ~ENABLE_ECHO_INPUT);
  std::string s = ask(prompt);
  SetConsoleMode(h, mode);
#else
  termios old{};
  tcgetattr(STDIN_FILENO, &old);
  termios quiet = old;
  quiet.c_lflag &= ~static_cast<tcflag_t>(ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &quiet);
  std::string s = ask(prompt);
  tcsetattr(STDIN_FILENO, TCSANOW, &old);
#endif
  out_ << "\n";
  return s;
}

int ConsoleApp::askInt(const std::string& prompt) {
  const std::string t = ask(prompt);
  try {
    std::size_t used = 0;
    const int v = std::stoi(t, &used);
    if (used != t.size()) throw std::invalid_argument("x");
    return v;
  } catch (const std::exception&) {
    throw ValidationError("Please enter a whole number.");
  }
}

double ConsoleApp::askNumber(const std::string& prompt) {
  const std::string t = ask(prompt);
  try {
    std::size_t used = 0;
    const double v = std::stod(t, &used);
    if (used != t.size()) throw std::invalid_argument("x");
    return v;
  } catch (const std::exception&) {
    throw ValidationError("Please enter a number, for example 14.5.");
  }
}

TimePoint ConsoleApp::askTime(const std::string& prompt) { return parseLocal(ask(prompt + " (YYYY-MM-DD HH:MM)")); }

void ConsoleApp::runMenu(const std::string& title, const std::vector<MenuItem>& items) {
  for (;;) {
    out_ << "\n=== " << title << " ===\n";
    for (std::size_t i = 0; i < items.size(); ++i) out_ << std::setw(3) << i + 1 << ". " << items[i].label << "\n";
    out_ << "  0. Log out\n";
    int choice = -1;
    try { choice = std::stoi(ask("Choose")); } catch (const std::invalid_argument&) {} catch (const std::out_of_range&) {}
    if (choice == 0) return;
    if (choice < 1 || choice > static_cast<int>(items.size())) { out_ << "Please choose a number from the list.\n"; continue; }
    guarded(items[static_cast<std::size_t>(choice) - 1].run);
  }
}

void ConsoleApp::guarded(const std::function<void()>& action) {
  try {
    action();
  } catch (const EndOfInput&) {
    throw;
  } catch (const LmsError& e) {
    out_ << "\n! " << e.what() << "\n";
  } catch (const std::exception& e) {
    out_ << "\n! Something unexpected went wrong and the action was cancelled (" << e.what() << ").\n";
  }
}

// ---------------- printing ----------------
void ConsoleApp::printCourses(const std::vector<Course>& courses) {
  if (courses.empty()) { out_ << "(no courses)\n"; return; }
  for (const auto& c : courses) out_ << std::setw(4) << c.id << "  " << std::left << std::setw(10) << c.code << c.title << std::right << "\n";
}

void ConsoleApp::printAssignments(const std::vector<AssignmentRow>& rows, bool showState) {
  if (rows.empty()) { out_ << "(no assignments)\n"; return; }
  for (const auto& r : rows) {
    out_ << std::setw(4) << r.assignment.id << "  " << std::left << std::setw(28) << r.assignment.title << std::right
         << " max " << std::setw(4) << r.assignment.maxMark << "  due " << formatLocal(r.assignment.deadline);
    if (r.extendedTo) out_ << "  (extended to " << formatLocal(*r.extendedTo) << ")";
    if (showState) out_ << "  status: " << (r.state ? toString(*r.state) : std::string("NOT SUBMITTED"));
    out_ << "\n";
  }
}

void ConsoleApp::printRows(const std::vector<SubmissionRow>& rows) {
  if (rows.empty()) { out_ << "(no submissions yet)\n"; return; }
  for (const auto& r : rows) {
    out_ << std::setw(4) << r.submission.id << "  " << std::left << std::setw(24) << r.studentName << std::right
         << std::setw(10) << toString(r.submission.state) << "  submitted " << formatLocal(r.submittedAt)
         << " (v" << r.versionCount << ")";
    if (r.mark)
      out_ << "  raw " << money(r.mark->rawMark) << " late " << r.mark->daysLate << "d final " << money(r.mark->finalMark);
    out_ << "\n";
  }
}

int ConsoleApp::chooseCourse(const User& u) {
  const auto courses = s_.assignments.coursesFor(u);
  printCourses(courses);
  if (courses.empty()) throw ValidationError("There are no courses to choose from.");
  return askInt("Course number");
}
int ConsoleApp::chooseAssignment(const User& u, int courseId) {
  printAssignments(s_.assignments.assignmentsFor(u, courseId), u.role() == Role::Student);
  return askInt("Assignment number");
}

// ---------------- main loop ----------------
void ConsoleApp::run() {
  out_ << "Moderated Learning Management System\n";
  try {
    for (;;) {
      out_ << "\nLog in (leave the username empty to quit)\n";
      const std::string username = ask("Username");
      if (username.empty()) return;
      const std::string password = askSecret("Password");
      try {
        const auto user = s_.auth.login(username, password);
        out_ << "\nWelcome, " << user->fullName() << " (" << displayName(user->role()) << ").\n";
        session(*user);
      } catch (const LmsError& e) {
        out_ << "\n! " << e.what() << "\n";
      }
    }
  } catch (const EndOfInput&) {
    out_ << "\n";
  }
}

void ConsoleApp::session(const User& user) {
  switch (user.role()) {
    case Role::Admin: adminMenu(user); break;
    case Role::Lecturer: lecturerMenu(user); break;
    case Role::Hod: hodMenu(user); break;
    case Role::Student: studentMenu(user); break;
  }
  out_ << "You are logged out.\n";
}

void ConsoleApp::changePassword(const User& u) {
  guarded([&] {
    const std::string oldPw = askSecret("Current password");
    const std::string newPw = askSecret("New password");
    s_.auth.changePassword(u, oldPw, newPw);
    out_ << "Your password was changed.\n";
  });
}

void ConsoleApp::showAudit(const User& u) {
  guarded([&] {
    AuditFilter f;
    for (const auto& e : s_.audit.list(u, f)) {
      out_ << "#" << e.id << " " << formatLocal(e.changedAt) << " submission " << e.submissionId << " by user "
           << e.changedBy << " " << toString(e.action) << ": "
           << (e.oldValue ? money(*e.oldValue) : std::string("none")) << " -> " << money(e.newValue) << " (" << e.reason << ")\n";
    }
  });
}

// ---------------- role menus (table-driven: one small action per menu line) ----------------
void ConsoleApp::createUserFlow(const User& admin) {
  out_ << "Roles: 1 Student, 2 Lecturer, 3 Head of department, 4 Administrator\n";
  const int r = askInt("Role");
  if (r < 1 || r > 4) throw ValidationError("Choose a role from 1 to 4.");
  const Role role = static_cast<Role>(r - 1);
  std::optional<int> dept;
  if (role == Role::Hod) {
    for (const auto& d : s_.admin.listDepartments(admin)) out_ << std::setw(4) << d.id << "  " << d.name << "\n";
    dept = askInt("Department number");
  }
  const std::string un = ask("Username"), fn = ask("Full name"), pw = askSecret("Initial password");
  out_ << "Account created with number " << s_.admin.createUser(admin, role, un, fn, pw, dept) << ".\n";
}

void ConsoleApp::adminMenu(const User& u) {
  runMenu("Administrator", {
      {"Create a user account", [&] { createUserFlow(u); }},
      {"List user accounts", [&] {
         for (const auto& x : s_.admin.listUsers(u))
           out_ << std::setw(4) << x->id() << "  " << std::left << std::setw(16) << x->username() << std::setw(26) << x->fullName()
                << std::setw(20) << displayName(x->role()) << std::right << (x->active() ? "active" : "INACTIVE") << "\n";
       }},
      {"Deactivate or activate an account", [&] {
         const int id = askInt("Account number");
         const std::string a = ask("Type D to deactivate or A to activate");
         s_.admin.setActive(u, id, !(a == "D" || a == "d"));
         out_ << "Done.\n";
       }},
      {"Create a department", [&] { s_.admin.createDepartment(u, ask("Department name")); out_ << "Department created.\n"; }},
      {"Create a course", [&] {
         for (const auto& d : s_.admin.listDepartments(u)) out_ << std::setw(4) << d.id << "  " << d.name << "\n";
         const int d = askInt("Department number");
         const std::string code = ask("Course code"), title = ask("Course title");
         s_.admin.createCourse(u, code, title, d);
         out_ << "Course created.\n";
       }},
      {"Enrol a student in a course", [&] {
         const int s = askInt("Student account number"); const int co = askInt("Course number");
         s_.admin.enrol(u, s, co); out_ << "Student enrolled.\n";
       }},
      {"Assign a lecturer to a course", [&] {
         const int l = askInt("Lecturer account number"); const int co = askInt("Course number");
         s_.admin.assignLecturer(u, l, co); out_ << "Lecturer assigned.\n";
       }},
      {"List departments and courses", [&] {
         for (const auto& d : s_.admin.listDepartments(u)) out_ << "Department " << d.id << ": " << d.name << "\n";
         printCourses(s_.admin.listCourses(u));
       }},
      {"View the audit log", [&] { showAudit(u); }},
      {"Back up the database", [&] { s_.admin.backup(u, ask("Backup file name")); out_ << "Backup saved.\n"; }},
      {"Change my password", [&] { changePassword(u); }}});
}

void ConsoleApp::lecturerCourses(const User& u) {
  for (const auto& co : s_.assignments.coursesFor(u)) {
    out_ << co.code << " " << co.title << "\n";
    printAssignments(s_.assignments.assignmentsFor(u, co.id), u.role() == Role::Student);
  }
}

void ConsoleApp::lecturerMenu(const User& u) {
  runMenu("Lecturer", {
      {"My courses and assignments", [&] { lecturerCourses(u); }},
      {"Create an assignment", [&] {
         const int course = chooseCourse(u);
         const std::string title = ask("Title"), desc = ask("Description");
         const TimePoint due = askTime("Deadline");
         const int max = askInt("Maximum mark (1 to 1000)");
         out_ << "Assignment created with number " << s_.assignments.createAssignment(u, course, title, desc, due, max) << ".\n";
       }},
      {"Grant an extension to a student", [&] {
         const int course = chooseCourse(u);
         const int a = chooseAssignment(u, course);
         for (const auto& st : s_.assignments.studentsIn(u, course)) out_ << std::setw(4) << st.id << "  " << st.username << "  " << st.fullName << "\n";
         const int student = askInt("Student number");
         const TimePoint when = askTime("New deadline");
         s_.assignments.grantExtension(u, a, student, when, ask("Reason"));
         out_ << "Extension granted.\n";
       }},
      {"View submissions of an assignment", [&] { const int co = chooseCourse(u); printRows(s_.assignments.listSubmissions(u, chooseAssignment(u, co))); }},
      {"Mark a submission", [&] {
         const int sub = askInt("Submission number");
         const double raw = askNumber("Mark before late penalty");
         const std::string fb = ask("Written feedback");
         const auto r = s_.marking.mark(u, sub, raw, fb);
         out_ << "Saved. Days late: " << r.penalty.daysLate << ", penalty " << money(r.penalty.penalty)
              << ", final mark " << money(r.penalty.finalMark) << ". The student cannot see it until the head of department releases it.\n";
       }},
      {"Re-mark a submission", [&] {
         const int sub = askInt("Submission number");
         const double raw = askNumber("New mark before late penalty");
         const std::string why = ask("Reason for the change");
         out_ << "Saved. Final mark is now " << money(s_.marking.remark(u, sub, raw, why).penalty.finalMark) << ".\n";
       }},
      {"Grant an extension to a whole class", [&] {
         const int course = chooseCourse(u);
         const int a = chooseAssignment(u, course);
         const TimePoint when = askTime("New deadline for the whole class");
         const ClassExtensionResult r = s_.assignments.grantClassExtension(u, a, when, ask("Reason"));
         out_ << "Extension granted to " << r.granted << " student(s).";
         if (r.kept) out_ << " " << r.kept << " student(s) already had a later extension and keep it.";
         out_ << "\n";
       }},
      {"Change my password", [&] { changePassword(u); }}});
}

void ConsoleApp::batchFlow(const User& hod, bool approve) {
  const int co = chooseCourse(hod);
  const int a = chooseAssignment(hod, co);
  const auto rows = s_.moderation.listForModeration(hod, a);
  printRows(rows);
  const std::string which = ask("Submission number, or ALL");
  const SubmissionState want = approve ? SubmissionState::Marked : SubmissionState::Moderated;
  std::vector<int> ids;
  if (which == "ALL" || which == "all") {
    for (const auto& r : rows) if (r.submission.state == want) ids.push_back(r.submission.id);
  } else {
    try { ids.push_back(std::stoi(which)); } catch (const std::exception&) { throw ValidationError("Please enter a whole number or ALL."); }
  }
  const BatchResult r = approve ? s_.moderation.approve(hod, ids) : s_.moderation.release(hod, ids);
  out_ << (approve ? "Approved " : "Released ") << r.done << " mark(s)";
  if (r.skipped) out_ << "; skipped " << r.skipped << " that were not in the right state";
  out_ << ".\n";
}

void ConsoleApp::hodMenu(const User& u) {
  runMenu("Head of Department", {
      {"Courses and assignments in my department", [&] { lecturerCourses(u); }},
      {"Review marks of an assignment", [&] { const int co = chooseCourse(u); printRows(s_.moderation.listForModeration(u, chooseAssignment(u, co))); }},
      {"Adjust a mark before approval", [&] {
         const int sub = askInt("Submission number");
         const double mark = askNumber("New final mark");
         s_.moderation.adjustMark(u, sub, mark, ask("Reason"));
         out_ << "Mark adjusted and recorded.\n";
       }},
      {"Approve marks", [&] { batchFlow(u, true); }},
      {"Release approved marks", [&] { batchFlow(u, false); }},
      {"Correct a released mark", [&] {
         const int sub = askInt("Submission number");
         const double mark = askNumber("Corrected final mark");
         s_.moderation.correctReleasedMark(u, sub, mark, ask("Reason"));
         out_ << "Mark corrected and recorded.\n";
       }},
      {"View the audit log", [&] { showAudit(u); }},
      {"Change my password", [&] { changePassword(u); }}});
}

void ConsoleApp::studentMenu(const User& u) {
  runMenu("Student", {
      {"My courses and assignments", [&] { lecturerCourses(u); }},
      {"Submit work", [&] {
         const int co = chooseCourse(u);
         const int a = chooseAssignment(u, co);
         const Receipt r = s_.submissions.submit(u, a, ask("Path of the file to submit"));
         out_ << "Submitted. Version " << r.versionNo << " received " << formatLocal(r.submittedAt) << ".";
         if (r.state == SubmissionState::Late) out_ << " This work is late by " << r.daysLate << " day(s); a penalty applies.";
         out_ << "\n";
       }},
      {"See a released mark and feedback", [&] {
         const int co = chooseCourse(u);
         const auto m = s_.submissions.releasedMark(u, chooseAssignment(u, co));
         if (!m) out_ << "No mark has been released for this assignment yet.\n";
         else out_ << "Final mark: " << money(m->finalMark) << " (days late: " << m->daysLate << ")\nFeedback: " << m->feedback << "\n";
       }},
      {"Change my password", [&] { changePassword(u); }}});
}

}  // namespace lms
