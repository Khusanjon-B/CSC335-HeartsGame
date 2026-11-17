// ---------------------------------------------------------------------------
// PasswordHasher.cpp - see PasswordHasher.h.
// ---------------------------------------------------------------------------

#include "PasswordHasher.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QRandomGenerator>

namespace {

// SHA-256(salt + secret), returned as hex text.
QString sha256Hex(const QString& salt, const QString& secret)
{
    const QByteArray input = (salt + secret).toUtf8();
    return QString::fromLatin1(
        QCryptographicHash::hash(input, QCryptographicHash::Sha256).toHex());
}

// 16 random bytes as 32 hex characters. QRandomGenerator::system() uses the
// operating system's secure random source.
QString randomSalt()
{
    QByteArray bytes;
    for (int i = 0; i < 16; ++i)
        bytes.append(static_cast<char>(QRandomGenerator::system()->bounded(256)));
    return QString::fromLatin1(bytes.toHex());
}

} // namespace

namespace PasswordHasher
{

Hashed hash(const QString& secret)
{
    Hashed result;
    result.salt = randomSalt();
    result.hash = sha256Hex(result.salt, secret);
    return result;
}

bool verify(const QString& secret, const QString& expectedHash, const QString& salt)
{
    return sha256Hex(salt, secret) == expectedHash;
}

}
