// Shared types: time, errors and the Clock abstraction.
#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>

namespace lms {

using TimePoint = std::int64_t;  // seconds since 1970-01-01 00:00:00 UTC
constexpr TimePoint kDay = 86400;

struct LmsError : std::runtime_error { using std::runtime_error::runtime_error; };
struct ValidationError : LmsError { using LmsError::LmsError; };
struct AccessDeniedError : LmsError { using LmsError::LmsError; };
struct RuleViolation : LmsError { using LmsError::LmsError; };
struct NotFoundError : LmsError { using LmsError::LmsError; };
struct DatabaseError : LmsError { using LmsError::LmsError; };

class Clock {
 public:
  virtual ~Clock() = default;
  virtual TimePoint now() const = 0;
};

class SystemClock : public Clock {
 public:
  TimePoint now() const override;
};

class FixedClock : public Clock {
 public:
  explicit FixedClock(TimePoint t) : t_(t) {}
  void set(TimePoint t) { t_ = t; }
  TimePoint now() const override { return t_; }
 private:
  TimePoint t_;
};

std::string toIso(TimePoint t);                 // "2026-10-20T14:30:00Z"
TimePoint fromIso(const std::string& s);        // throws ValidationError
std::string formatLocal(TimePoint t);           // "2026-10-20 16:30" in local time
TimePoint parseLocal(const std::string& s);     // "YYYY-MM-DD HH:MM" local time; throws ValidationError
std::string trim(const std::string& s);

}  // namespace lms
