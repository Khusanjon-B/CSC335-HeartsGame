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
inline const QString InvalidUsername  = "INVALID_USERNAME";    // not just letters, digits, _ and -
inline const QString InvalidPassword  = "INVALID_PASSWORD";    // too short, or missing a letter/digit/special char (also recover_password)
// (a taken username or email is reported as InvalidCredentials, as in the protocol doc)

// ---- login ----
inline const QString InvalidCredentials = "INVALID_CREDENTIALS"; // wrong username or password (deliberately the same code)
inline const QString AlreadyLoggedIn    = "ALREADY_LOGGED_IN";   // this connection is already logged in
inline const QString AccountInUse       = "ACCOUNT_IN_USE";      // account is logged in on another connection
inline const QString AccountLocked      = "ACCOUNT_LOCKED";      // 3 failed logins; locked for 5 min or until an admin unlocks

// ---- logout ----
inline const QString NotLoggedIn      = "NOT_LOGGED_IN";       // nothing to log out of

// ---- lobbies / game sessions (use cases 8-11; names from the protocol doc) ----
inline const QString CouldNotCreateLobby = "COULD_NOT_CREATE_LOBBY"; // max concurrent games reached (server.ini)
inline const QString LobbyNotFound       = "LOBBY_NOT_FOUND";        // no lobby with that lobbyId
inline const QString LobbyFull           = "LOBBY_FULL";             // already 4 players
inline const QString GameAlreadyStarted  = "GAME_ALREADY_STARTED";   // lobby closed when the game started
inline const QString AlreadyInLobby      = "ALREADY_IN_LOBBY";       // leave your current lobby first
inline const QString NotInLobby          = "NOT_IN_LOBBY";           // leave/start without being in a lobby
inline const QString NotHost             = "NOT_HOST";               // only the host can start the game
inline const QString LobbyNotFull        = "LOBBY_NOT_FULL";         // need 4 players to start

// ---- in-game: passing (use case 17) ----
inline const QString NotInGame        = "NOT_IN_GAME";         // not seated at a started game
inline const QString NotPassingPhase  = "NOT_PASSING_PHASE";   // passing is over, or this is a hold round
inline const QString AlreadyPassed    = "ALREADY_PASSED";      // you already picked your 3 cards
inline const QString InvalidCards     = "INVALID_CARDS";       // not 3 cards, bad code, not in your hand, or a duplicate

// ---- recover_password ----
inline const QString RecoveryFailed   = "RECOVERY_FAILED";     // username/email/pin don't match

// ---- change_password (use case 4) ----
// (a wrong current password is INVALID_CREDENTIALS; a weak new one is INVALID_PASSWORD)
inline const QString SamePassword     = "SAME_PASSWORD";       // new password is the same as the current one
}
