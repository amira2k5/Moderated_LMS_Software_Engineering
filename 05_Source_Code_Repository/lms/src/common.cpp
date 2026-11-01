#include "common.hpp"

#include <chrono>
#include <cstdio>
#include <ctime>

namespace lms {

TimePoint SystemClock::now() const {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string toIso(TimePoint t) {
  std::time_t tt = static_cast<std::time_t>(t);
  std::tm tm{};
  gmtime_r(&tt, &tm);
  char buf[32];
  std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", &tm);
  return buf;
}

TimePoint fromIso(const std::string& s) {
  int y, mo, d, h, mi, se;
  char z = 0;
  if (std::sscanf(s.c_str(), "%4d-%2d-%2dT%2d:%2d:%2d%c", &y, &mo, &d, &h, &mi, &se, &z) != 7 || z != 'Z')
    throw ValidationError("Invalid stored time: " + s);
  std::tm tm{};
  tm.tm_year = y - 1900; tm.tm_mon = mo - 1; tm.tm_mday = d;
  tm.tm_hour = h; tm.tm_min = mi; tm.tm_sec = se;
  return static_cast<TimePoint>(timegm(&tm));
}

std::string formatLocal(TimePoint t) {
  std::time_t tt = static_cast<std::time_t>(t);
  std::tm tm{};
  localtime_r(&tt, &tm);
  char buf[32];
  std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M", &tm);
  return buf;
}

TimePoint parseLocal(const std::string& s) {
  int y, mo, d, h, mi;
  if (std::sscanf(s.c_str(), "%4d-%2d-%2d %2d:%2d", &y, &mo, &d, &h, &mi) != 5 || mo < 1 || mo > 12 ||
      d < 1 || d > 31 || h < 0 || h > 23 || mi < 0 || mi > 59)
    throw ValidationError("Please enter the date and time as YYYY-MM-DD HH:MM, for example 2026-11-20 17:00.");
  std::tm tm{};
  tm.tm_year = y - 1900; tm.tm_mon = mo - 1; tm.tm_mday = d; tm.tm_hour = h; tm.tm_min = mi;
  tm.tm_isdst = -1;
  return static_cast<TimePoint>(std::mktime(&tm));
}

std::string trim(const std::string& s) {
  const auto b = s.find_first_not_of(" \t\r\n");
  if (b == std::string::npos) return "";
  return s.substr(b, s.find_last_not_of(" \t\r\n") - b + 1);
}

}  // namespace lms
