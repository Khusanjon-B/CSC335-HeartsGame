# Hearts Server — Current State

_Last updated: 2026-11-09_

A Qt 6 console app that accepts TCP connections from Hearts clients, handles
accounts (register / login / logout), and stores players in a SQLite database.
No game logic yet.

---

## Status at a glance

| Feature | Status |
|---|---|
| TCP server (multiple clients) | ✅ Working |
| JSON message protocol | ✅ Working |
| Register / login / logout | ✅ Working (tested) |
| SQLite `players` table | ✅ Working |
| Console broadcast + `exit` | ✅ Working |
| Password hashing | ❌ Not yet (plain text, for debugging) |
| Lobby / matchmaking | ❌ Not started |
| Hearts game logic | ❌ Not started |

---

## Building and running

- **Qt:** 6.11.1, MinGW 64-bit (needs the `Core`, `Network`, and `Sql` modules)
- **Build:** open `CMakeLists.txt` in Qt Creator and run it
- **Executable:** `test_server [port]`, default port **5000**
- **Database file:** `hearts.db`, created next to the `.exe` (inside `build/`, which git ignores).
  Delete it to start over with an empty database.

**Console commands** (type them into the server window):
- any text → broadcast to every client as a `server_message`
- `exit` → logs everyone out in the DB and shuts down

---

## File layout

| File | What it does |
|---|---|
| `main.cpp` | Entry point. Reads the port, opens the DB, starts the server and the console thread |
| `Server.h/.cpp` | TCP server, the `m_connections` list, JSON parsing, command handlers |
| `Database.h/.cpp` | All SQL. Every DB read and write goes through this class |
| `Connection.h` | `struct Connection`: everything the server knows about one client |
| `Log.h` | `logMessage()`, a timestamped console logging helper |

---

## How it works

### Threads
- **Main thread:** runs Qt's event loop, which handles all networking and all database work.
  Qt calls `onNewConnection` / `onReadyRead` / `onDisconnected` when things happen.
  These run one at a time, so no locks are needed.
- **Console thread:** blocks on `std::getline` and hands work to the main thread
  with `QMetaObject::invokeMethod`. It never touches server data directly.

### Connection lifecycle
1. Client connects → a `Connection` is added to `m_connections` (anonymous, `verified = false`).
2. `login` succeeds → `playerId`, `username`, and `verified` are filled in, and the DB row is marked connected.
3. `logout` → the fields are cleared and the DB row is marked disconnected. The socket stays open.
4. Socket disconnects → the server logs them out if needed and removes them from `m_connections`.

### `struct Connection`
| Field | Type | Meaning |
|---|---|---|
| `socket` | `QTcpSocket*` | The client's socket |
| `ip` | `QString` | `"ip:port"`, used in log lines |
| `playerId` | `std::optional<int>` | DB `player_id`, empty until login |
| `username` | `QString` | Empty until login |
| `verified` | `bool` | `true` once logged in |

---

## Message protocol

One **JSON object per line**, ending in `\n`. Every message has a `"command"` field.

### Client → server

| Command | Fields | Reply command |
|---|---|---|
| `register` | `username`, `password` | `register_result` |
| `login` | `username`, `password` | `login_result` |
| `logout` | _(none)_ | `logout_result` |

### Server → client

| Command | Fields | When |
|---|---|---|
| `register_result` | `success`, `message`, `playerId` (on success) | Reply to `register` |
| `login_result` | `success`, `message`, `playerId` + `username` (on success) | Reply to `login` |
| `logout_result` | `success`, `message` | Reply to `logout` |
| `server_message` | `text` | Someone typed in the server console |
| `error` | `message` | Invalid JSON, not an object, or an unknown command |

### Examples
```
> {"command":"register","username":"alice","password":"pw"}
< {"command":"register_result","message":"account created, you can now log in","playerId":1,"success":true}

> {"command":"login","username":"alice","password":"pw"}
< {"command":"login_result","message":"welcome alice","playerId":1,"success":true,"username":"alice"}

> {"command":"dance"}
< {"command":"error","message":"unknown command: \"dance\""}
```

### Rules
- `register` does **not** log you in. Send `login` afterward.
- Only one connection can be logged in to an account at a time.
- A wrong password and an unknown username both return `"wrong username or password"`.
- `login` while already logged in on that connection fails.

### Adding a new command
1. Declare `void handleXxx(Connection& conn, const QJsonObject& msg);` in `Server.h`.
2. Implement it in `Server.cpp` (read fields from `msg`, reply with `sendJson`).
3. Add `m_handlers["xxx"] = &Server::handleXxx;` in `registerHandlers()`.

---

## Database

SQLite through Qt's built-in `QSQLITE` driver, so no separate SQLite install is needed.

### Table `players`
| Column | Type | Notes |
|---|---|---|
| `player_id` | INTEGER PK AUTOINCREMENT | |
| `username` | TEXT NOT NULL UNIQUE | |
| `password` | TEXT NOT NULL | ⚠️ plain text for now |
| `last_logged_in` | TEXT | ISO timestamp, e.g. `2026-11-09T23:41:07` |
| `last_logged_out` | TEXT | ISO timestamp |
| `connected` | INTEGER | 0 or 1. Reset to 0 for everyone at server startup |

After changing the columns in `Database::createTables()`, delete `hearts.db` so it gets recreated.

---

## Known issues / TODO

- [ ] Hash passwords (salted, e.g. `QCryptographicHash`). Right now they're stored and compared in plain text.
- [ ] Stop logging raw incoming lines. The log currently prints passwords.
- [ ] Clients need updating: they still expect the old plain-text `ack:` / `server:` replies.
- [ ] Lobby / tables / seating
- [ ] Hearts game logic

---

## Change log

### 2026-11-09 — Login system + restructure
- Split the single `main.cpp` into `Server`, `Database`, `Connection`, and `Log`.
- Added the JSON protocol with `register` / `login` / `logout`.
- Added the SQLite `players` table.
- Fixed a crash on exit: the console thread was never joined or detached.
- Replaced the plain-text `ack:` / `server:` replies with JSON.

### Earlier
- Basic TCP echo server with a console broadcast.
