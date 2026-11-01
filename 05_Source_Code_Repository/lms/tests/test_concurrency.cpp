// NFR-10: up to 5 simultaneous sessions must not corrupt data.
#include <atomic>
#include <thread>

#include "fixture.hpp"

using namespace lms;
using namespace lms::test;

TEST(Concurrency, FiveSessionsWriteToOneDatabaseFileWithoutCorruption) {
  TempDir tmp;
  const std::string path = (tmp.path / "shared.db").string();
  { Database init(path); }  // creates the schema
  constexpr int kSessions = 5, kWrites = 20;
  std::atomic<int> failures{0};
  std::vector<std::thread> sessions;
  for (int s = 0; s < kSessions; ++s) {
    sessions.emplace_back([&, s] {
      try {
        Database db(path);  // each session has its own connection, like a separate program instance
        SqliteCourseRepository courses(db);
        SqliteUnitOfWork uow(db);
        for (int i = 0; i < kWrites; ++i) {
          auto tx = uow.begin();
          courses.addDepartment("Dept " + std::to_string(s) + "-" + std::to_string(i));
          tx->commit();
        }
      } catch (const std::exception&) {
        failures++;
      }
    });
  }
  for (auto& t : sessions) t.join();
  EXPECT_EQ(failures.load(), 0);
  Database check(path);
  SqliteCourseRepository courses(check);
  EXPECT_EQ(courses.listDepartments().size(), static_cast<std::size_t>(kSessions * kWrites));
  auto st = check.prepare("PRAGMA integrity_check");
  ASSERT_TRUE(st.step());
  EXPECT_EQ(st.text(0), "ok");
}
