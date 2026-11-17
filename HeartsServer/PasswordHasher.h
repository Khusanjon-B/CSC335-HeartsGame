#pragma once
// ---------------------------------------------------------------------------
// PasswordHasher.h - salted SHA-256 hashing for passwords and recovery pins.
//
// WHY HASH: the database never stores the real password. It stores
//   hash = SHA-256(salt + password)
// When someone logs in, we hash what they typed with the same salt and
// compare the results. If the DB file leaks, the real passwords aren't in it.
//
// WHY SALT: the salt is a random string made once per password and stored
// next to the hash. Two players who both pick "password123" end up with
// different hashes, so you can't tell they share a password or look the
// hash up in a precomputed table.
//
// This is deliberately simple (one SHA-256 pass). Real production systems
// use slow hashes like bcrypt or Argon2. That's more than this project needs.
// ---------------------------------------------------------------------------

#include <QString>

namespace PasswordHasher
{
// What gets stored in the database for one secret (password or pin).
// Both are hex text, e.g. "9f86d08188...".
struct Hashed
{
    QString hash;
    QString salt;
};

// Makes a new random salt and hashes `secret` with it.
// Use this when creating or changing a password or pin.
Hashed hash(const QString& secret);

// True if `secret` hashed with `salt` equals `expectedHash`.
// Use this when checking a login or a recovery pin.
bool verify(const QString& secret, const QString& expectedHash, const QString& salt);
}
