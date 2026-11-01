#include "security.hpp"

#include <sodium.h>

#include "common.hpp"

namespace lms {

LibsodiumHasher::LibsodiumHasher(Strength s) : strength_(s) {
  if (sodium_init() < 0) throw LmsError("The password library could not be started.");
}

std::string LibsodiumHasher::hash(const std::string& password) const {
  char out[crypto_pwhash_STRBYTES];
  const auto ops = strength_ == Strength::Interactive ? crypto_pwhash_OPSLIMIT_INTERACTIVE : crypto_pwhash_OPSLIMIT_MIN;
  const auto mem = strength_ == Strength::Interactive ? crypto_pwhash_MEMLIMIT_INTERACTIVE : crypto_pwhash_MEMLIMIT_MIN;
  if (crypto_pwhash_str(out, password.c_str(), password.size(), ops, mem) != 0)
    throw LmsError("The password could not be protected (out of memory).");
  return out;
}

bool LibsodiumHasher::verify(const std::string& password, const std::string& hash) const {
  return crypto_pwhash_str_verify(hash.c_str(), password.c_str(), password.size()) == 0;
}

}  // namespace lms
