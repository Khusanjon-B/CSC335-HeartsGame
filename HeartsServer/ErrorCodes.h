#pragma once
// ---------------------------------------------------------------------------
// ErrorCodes.h - every error code the server can send, in one place.
//
// Every "*_result" reply has an "error" field:
//   - ""            on success (so clients can always read the field)
//   - one of these  on failure
// The standalone {"command":"error"} message uses these codes too.
//
// Clients should switch on the code, not on the human-readable "message"
// text, because the text may change but these codes won't.
//
// To add a code: add a line here, use it in Server.cpp, and add it to the
// error-code table in PROTOCOL.md.
// ---------------------------------------------------------------------------

#include <QString>

namespace ErrorCode
{
// ---- problems with the message itself (sent as {"command":"error"}) ----
inline const QString InvalidJson      = "INVALID_JSON";        // not valid JSON / not an object
inline const QString MissingRequestId = "MISSING_REQUEST_ID";  // requestId missing or not a positive whole number
inline const QString UnknownCommand   = "UNKNOWN_COMMAND";     // no handler for this "command"

// ---- shared by several commands ----
inline const QString None             = "";                    // success
inline const QString MissingFields    = "MISSING_FIELDS";      // a required field is blank or missing
inline const QString ServerError      = "SERVER_ERROR";        // database problem on our side

// ---- register ----
inline const QString InvalidEmail     = "INVALID_EMAIL";       // doesn't look like x@y.z
inline const QString InvalidPin       = "INVALID_PIN";         // not a 4-8 digit string
// (a taken username is reported as InvalidCredentials, as in the protocol doc)

// ---- login ----
inline const QString InvalidCredentials = "INVALID_CREDENTIALS"; // wrong username or password (deliberately the same code)
inline const QString AlreadyLoggedIn    = "ALREADY_LOGGED_IN";   // this connection is already logged in
inline const QString AccountInUse       = "ACCOUNT_IN_USE";      // account is logged in on another connection

// ---- logout ----
inline const QString NotLoggedIn      = "NOT_LOGGED_IN";       // nothing to log out of

// ---- recover_password ----
inline const QString RecoveryFailed   = "RECOVERY_FAILED";     // username/email/pin don't match
}
