# Hearts Server — Current State

_Last updated: 2026-11-16_

A Qt 6 console app that accepts TCP connections from Hearts clients, handles
accounts (register / login / logout / password recovery), and stores players in
a SQLite database. No game logic yet.

**Manual test checklist:** [TESTING.md](TESTING.md)

---

## Status at a glance

| Feature | Status |
|---|---|
| TCP server (multiple clients) | ✅ Working |
| JSON message protocol with `requestId` | ✅ Working |
| Register (username, email, password, pin) | ✅ Working (tested) |
| Login / logout | ✅ Working (tested) |
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

One **JSON object per line**, ending in `\n`. Every message has `"command"` and `"requestId"`.

### requestId rules
- **Clients** must send a positive whole number (1, 2, 3, …), usually a counter.
- **Replies** copy the request's `requestId`, so the client can match them up.
- **Server-initiated messages** (broadcasts) use **negative** ids (-1, -2, …).
- **0** means "unknown". The server uses it when replying to a message with no readable id.

### Client → server

| Command | Fields | Reply command |
|---|---|---|
| `register` | `username`, `email`, `password`, `pin` | `register_result` |
| `login` | `username`, `password` | `login_result` |
| `logout` | _(none)_ | `logout_result` |
| `recover_password` | `username`, `email`, `pin`, `newPassword` | `recover_password_result` |

### Server → client

| Command | Fields | When |
|---|---|---|
| `<command>_result` | `requestId`, `success`, `message` (+ extras below) | Reply to every command |
| ↳ `register_result` extras | `playerId` | On success |
| ↳ `login_result` extras | `playerId`, `username` | On success |
| `server_message` | `requestId` (negative), `text` | Someone typed in the server console |
| `error` | `requestId`, `message` | Invalid JSON, a bad or missing requestId, or an unknown command |

### Examples
```
> {"command":"register","requestId":1,"username":"alice","email":"alice@example.com","password":"pw","pin":"0042"}
< {"command":"register_result","message":"account created, you can now log in","playerId":1,"requestId":1,"success":true}

> {"command":"login","requestId":2,"username":"alice","password":"pw"}
< {"command":"login_result","message":"welcome alice","playerId":1,"requestId":2,"success":true,"username":"alice"}

> {"command":"recover_password","requestId":3,"username":"alice","email":"alice@example.com","pin":"0042","newPassword":"new"}
< {"command":"recover_password_result","message":"password changed, you can now log in","requestId":3,"success":true}

< {"command":"server_message","requestId":-1,"text":"game starts soon"}

> {"command":"login","username":"alice","password":"pw"}
< {"command":"error","message":"every message needs a \"requestId\" that is a positive whole number","requestId":0}
```

### Rules
- **Pin:** a **string** of 4–8 digits (`"0042"`, not `42`), so leading zeros aren't lost. Limits are in `Server.cpp`.
- **Email:** must look like `x@y.z`. It's stored lowercase, and recovery compares it case-insensitively.
- **Register** does **not** log you in. Send `login` afterward.
- **One login per account:** only one connection can be logged in to an account at a time.
- **Login failures:** a wrong password and an unknown username both return `"wrong username or password"`.
- **Recovery failures:** a wrong username, email, or pin all return the same message.
- **JSON key order** in replies is alphabetical. Read fields by name.

### Adding a new command
1. Declare `void handleXxx(Connection& conn, const Message& request);` in `Server.h`.
2. Implement it in `Server.cpp`: read from `request.body`, then call `replyResult(conn, request, success, "text", {extras})`.
3. Add `m_handlers["xxx"] = &Server::handleXxx;` in `registerHandlers()`.
4. If it has a secret field, add the field name to `redactSecrets()` in `Message.h`.

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

- [ ] Clients need updating to the new protocol (`requestId`, the new register fields, JSON replies).
- [ ] No limit on wrong pin attempts. A 4-digit pin could be guessed by trying them all.
- [ ] Emails aren't unique, and there's no email verification.
- [ ] `hearts.db` and the logs live in the build folder; deleting the build folder deletes them.
- [ ] Lobby / tables / seating
- [ ] Hearts game logic

---

## Change log

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
