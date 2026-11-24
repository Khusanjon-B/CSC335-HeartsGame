# Hearts Server — Current State

_Last updated: 2026-11-19_

A Qt 6 console app that accepts TCP connections from Hearts clients, handles
accounts (register / login / logout / password recovery), and stores players in
a SQLite database. No game logic yet.

**Message protocol:** [PROTOCOL.md](PROTOCOL.md) · **Manual test checklist:** [TESTING.md](TESTING.md)

---

## Status at a glance

| Feature | Status |
|---|---|
| TCP server (multiple clients) | ✅ Working |
| JSON message protocol with `requestId` | ✅ Working |
| Register (username, email, password, pin) | ✅ Working (tested) |
| Login / logout (issue #59) | ✅ Working (tested) |
| `error` code in every reply | ✅ Working (tested) |
| Failed-login tracking / lockout (issue #54) | ❌ Not started |
| Password recovery with a pin | ✅ Working (tested) |
| Salted SHA-256 for passwords and pins | ✅ Working |
| Per-connection message history | ✅ Working (last 100 messages) |
| Log file + terminal logging | ✅ Working |
| Console broadcast + `exit` | ✅ Working |
| Lobby / matchmaking | ❌ Not started |
| Hearts game logic | ❌ Not started |

---

## Building and running

- **Qt:** 6.11.1, MinGW 64-bit (needs the `Core`, `Network`, and `Sql` modules)
- **Build:** open `CMakeLists.txt` in Qt Creator and run it
- **Executable:** `test_server [port]`, default port **5000**
- **Files the server creates next to the `.exe`** (inside `build/`, which git ignores):
  - `hearts.db`: the database. Delete it to start over.
  - `logs/server-YYYY-MM-DD.log`: one log file per day, appended to on each run.

**Console commands** (type them into the server window):
- any text → broadcast to every client as a `server_message`
- `exit` → logs everyone out in the DB and shuts down

---

## File layout

| File | What it does |
|---|---|
| `main.cpp` | Entry point. Opens the log file and the DB, starts the server and the console thread |
| `Server.h/.cpp` | TCP server, the `m_connections` list, JSON parsing, command handlers, sending |
| `Database.h/.cpp` | All SQL. Every DB read and write goes through this class |
| `PasswordHasher.h/.cpp` | Salted SHA-256: `hash()` for new secrets, `verify()` for checking them |
| `Message.h` | `struct Message`, the requestId rules, and `redactSecrets()` |
| `ErrorCodes.h` | Every error code the server can send |
| `PROTOCOL.md` | The message protocol spec, written to match the team Google Doc |
| `Connection.h` | `struct Connection`: one connected client, including its message history |
| `Log.h/.cpp` | `logMessage()` writes to the terminal and the log file |

---

## How it works

### Threads
- **Main thread:** runs Qt's event loop, which handles all networking and all database work.
  Qt calls `onNewConnection` / `onReadyRead` / `onDisconnected` when things happen.
  These run one at a time, so no locks are needed.
- **Console thread:** blocks on `std::getline` and hands work to the main thread
  with `QMetaObject::invokeMethod`. It never touches server data directly.

### What happens to one incoming line
`onReadyRead` → `processLine`:
1. Parse the line as JSON. If it fails, reply with `error` (requestId 0).
2. Wrap it in a `Message`, save it to `conn.history`, and log it (secrets shown as `***`).
3. Check `requestId` is a positive whole number. If not, reply with `error` (requestId 0).
4. Look up `command` in `m_handlers` and call the handler. Unknown commands get an `error`.

Every outgoing message goes through **`sendMessage`**, which adds `command` and
`requestId`, records the message in history, logs it, and writes it to the socket.

### Connection lifecycle
1. Client connects → a `Connection` is added to `m_connections` (anonymous, `verified = false`).
2. `login` succeeds → `playerId`, `username`, and `verified` are filled in, and the DB row is marked connected.
3. `logout` → the fields are cleared and the DB row is marked disconnected. The socket stays open.
4. Socket disconnects → the server logs them out if needed and removes them from `m_connections`.

### `struct Connection`
| Field | Type | Meaning |
|---|---|---|
| `socket` | `QTcpSocket*` | The client's socket |
| `ip` | `QString` | `"ip:port"` |
| `playerId` | `std::optional<int>` | DB `player_id`, empty until login |
| `username` | `QString` | Empty until login |
| `verified` | `bool` | `true` once logged in |
| `history` | `std::deque<Message>` | Last `kMaxHistory` (100) messages in both directions, secrets redacted |

### `struct Message`
| Field | Type | Meaning |
|---|---|---|
| `direction` | `FromClient` / `ToClient` | Which way it went |
| `requestId` | `qint64` | See the requestId rules below |
| `command` | `QString` | e.g. `"login"`, `"login_result"` |
| `body` | `QJsonObject` | The full JSON object |
| `time` | `QDateTime` | When the server received or sent it |

---

## Message protocol

**Full spec with every command, field and error code: [PROTOCOL.md](PROTOCOL.md).**

The short version:
- One JSON object per line, flat fields, and every message has `command` + `requestId` (lowercase d).
- Replies are `<command>_result` with `requestId` (echoed), `success`, `error`, and `message`.
- `error` is `""` on success, or a code from `ErrorCodes.h` on failure. Clients should branch on `error`, not on `message`.
- Server-initiated messages use negative requestIds. Replies to unreadable messages use 0.

| Command | Fields | Possible `error` codes |
|---|---|---|
| `register` | `username`, `email`, `password`, `pin` | `MISSING_FIELDS`, `INVALID_EMAIL`, `INVALID_PIN`, `INVALID_CREDENTIALS`, `SERVER_ERROR` |
| `login` | `username`, `password` | `MISSING_FIELDS`, `INVALID_CREDENTIALS`, `ALREADY_LOGGED_IN`, `ACCOUNT_IN_USE` |
| `logout` | _(none)_ | `NOT_LOGGED_IN` |
| `recover_password` | `username`, `email`, `pin`, `newPassword` | `MISSING_FIELDS`, `RECOVERY_FAILED`, `SERVER_ERROR` |
| _(server)_ `error` | `error`, `message` | `INVALID_JSON`, `MISSING_REQUEST_ID`, `UNKNOWN_COMMAND` |
| _(server)_ `server_message` | `text` | — |

### Adding a new command
1. Declare `void handleXxx(Connection& conn, const Message& request);` in `Server.h`.
2. Implement it in `Server.cpp`: read from `request.body`, then call
   `replyOk(conn, request, "text", {extras})` or `replyFail(conn, request, ErrorCode::Xxx, "text")`.
3. Add `m_handlers["xxx"] = &Server::handleXxx;` in `registerHandlers()`.
4. Add any new error codes to `ErrorCodes.h`.
5. If it has a secret field, add the field name to `redactSecrets()` in `Message.h`.
6. Document it in `PROTOCOL.md`, and add tests to `TESTING.md`.

---

## Passwords and pins

`PasswordHasher` stores `hash = SHA-256(salt + secret)` with a new random 16-byte salt for
every password and pin. Both are kept as hex text. Login and recovery hash the input with
the stored salt and compare the results. Plain-text passwords and pins are never stored or logged.

This is a single SHA-256 pass: simple, and good enough for a class project.
(Production systems use deliberately slow hashes like bcrypt.)

---

## Database

SQLite through Qt's built-in `QSQLITE` driver, so no separate SQLite install is needed.

### Table `players` (schema version 2)
| Column | Type | Notes |
|---|---|---|
| `player_id` | INTEGER PK AUTOINCREMENT | |
| `username` | TEXT NOT NULL UNIQUE | |
| `email` | TEXT NOT NULL | Lowercase |
| `password_hash` | TEXT NOT NULL | Hex SHA-256 |
| `password_salt` | TEXT NOT NULL | Hex |
| `pin_hash` | TEXT NOT NULL | Hex SHA-256 |
| `pin_salt` | TEXT NOT NULL | Hex |
| `last_logged_in` | TEXT | ISO timestamp, e.g. `2026-11-16T23:41:07` |
| `last_logged_out` | TEXT | ISO timestamp |
| `connected` | INTEGER | 0 or 1. Reset to 0 for everyone at server startup |

### Schema versions
The database file remembers its layout version in SQLite's `PRAGMA user_version`.
At startup, if the file is older than `kSchemaVersion` (in `Database.cpp`), the old
`players` table is **renamed to `players_old`** (kept, not deleted) and a new one is created.
Accounts from the old layout have to register again.

**To change the table:** edit the `CREATE TABLE` in `Database::prepareSchema()` and bump `kSchemaVersion`.

---

## Logging

`logMessage()` writes every line to the terminal (`hh:mm:ss`) and to
`logs/server-YYYY-MM-DD.log` (full date and time). Every message in or out is logged:

```
127.0.0.1:1211  >  #5 login {"password":"***","username":"alice"}
127.0.0.1:1211 (alice)  <  #5 login_result {"message":"welcome alice","playerId":1,"success":true,"username":"alice"}
```
`>` = from the client, `<` = to the client, `#5` = requestId. Secret fields are replaced
with `***`. The exception is lines that aren't valid JSON, which are logged as-is
(up to 200 characters) to help debug clients.

---

## Known issues / TODO

- [ ] Team Google Doc still shows `requestID` + `data`; paste in the sections from `PROTOCOL.md`.
- [ ] Failed-login counting and lockout, with `failed_attempts` in `login_result` (issue #54).
- [ ] No limit on wrong pin attempts. A 4-digit pin could be guessed by trying them all.
- [ ] Emails aren't unique, and there's no email verification.
- [ ] `hearts.db` and the logs live in the build folder; deleting the build folder deletes them.
- [ ] Lobby / tables / seating
- [ ] Hearts game logic

---

## Change log

### 2026-11-19 — Error codes + logout (issue #59)
- Every `*_result` now has an `error` field: `""` on success, a code on failure. `success` is derived from it.
- The standalone `error` message now carries a code too (`INVALID_JSON`, `MISSING_REQUEST_ID`, `UNKNOWN_COMMAND`).
- New `ErrorCodes.h` lists every code. Handlers use `replyOk` / `replyFail`.
- Logout uses `NOT_LOGGED_IN`, and its client-side behavior is documented.
- New `PROTOCOL.md`: the full protocol, ready to paste into the team Google Doc.

### 2026-11-16 — Hashing, requestId, recovery, logging
- Passwords and pins are stored as salted SHA-256 (`PasswordHasher`).
- `register` now requires `email` and a `pin`.
- New `recover_password` command (username + email + pin → new password).
- Every message carries a `requestId`. Replies echo it; server broadcasts use negative ids.
- New `struct Message`. Each `Connection` keeps its last 100 messages, with secrets redacted.
- Logging goes to the terminal and a daily log file. Passwords and pins are no longer logged.
- DB schema v2 with versioning. The old table is kept as `players_old`.
- Every outgoing message goes through one function, `sendMessage`.

### 2026-11-09 — Login system + restructure
- Split the single `main.cpp` into `Server`, `Database`, `Connection`, and `Log`.
- Added the JSON protocol with `register` / `login` / `logout`.
- Added the SQLite `players` table.
- Fixed a crash on exit: the console thread was never joined or detached.

### Earlier
- Basic TCP echo server with a console broadcast.
