// Password hashing behind an interface (libsodium Argon2id implementation).
#pragma once
#include <string>

namespace lms {

class IPasswordHasher {
 public:
  virtual ~IPasswordHasher() = default;
  virtual std::string hash(const std::string& password) const = 0;
  virtual bool verify(const std::string& password, const std::string& hash) const = 0;
};

class LibsodiumHasher : public IPasswordHasher {
 public:
  enum class Strength { Interactive, Fast };  // Fast is for automated tests only
  explicit LibsodiumHasher(Strength s = Strength::Interactive);
  std::string hash(const std::string& password) const override;
  bool verify(const std::string& password, const std::string& hash) const override;
 private:
  Strength strength_;
};

}  // namespace lms
