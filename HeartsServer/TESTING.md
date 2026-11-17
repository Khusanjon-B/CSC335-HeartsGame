# Hearts Server — Manual Test Cases

A running checklist of tests a person runs by hand to check the server.
**Add new tests whenever a server feature is added.** Don't delete old ones;
they're the regression suite.

_Last updated: 2026-11-16 (covers: connections, protocol, register, login/logout,
password recovery, hashing, history, logging, console, DB schema)_

---

## Setup

### 1. Start fresh
1. Stop the server if it's running.
2. In the build folder (next to `test_server.exe`), delete `hearts.db` and the `logs/` folder.
   Some tests below say **"with the old DB"**; skip this step for those.
3. Run the server from Qt Creator. Note the port (default 5000) and an IP it prints.

### 2. A way to send messages
You need something that can send one line of text over TCP. The easiest option uses Python,
which is already installed. Save this as `send.py` anywhere **outside** the repo:

```python
# Usage: python send.py [host] [port]
# Type one JSON message per line; replies print as they arrive. Ctrl+C to quit.
import socket, sys, threading
host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 5000
s = socket.create_connection((host, port))
def reader():
    for line in s.makefile("r"):
        print("<<", line.strip())
threading.Thread(target=reader, daemon=True).start()
for line in sys.stdin:
    s.sendall(line.encode())
```

Run `python send.py`, then paste the messages from the tests. For tests that need
**two clients**, open two terminals and run `send.py` in each.

> Tip: keep incrementing `requestId` yourself (1, 2, 3, ...) like a real client would.

### 3. Looking at the database
Open `hearts.db` with [DB Browser for SQLite](https://sqlitebrowser.org/), or run
`python -c "import sqlite3; print(sqlite3.connect('hearts.db').execute('select * from players').fetchall())"`.

### Recording results
Fill in the **Result** column with ✅ / ❌ and the date, e.g. `✅ 11-16`. If a test fails,
add a note under [Failures](#failures).

---

## A. Startup and connections

| ID | Steps | Expected | Result |
|---|---|---|---|
| A1 | Start the server | Terminal shows the log-file path, `[DB] opened ...hearts.db`, `listening on port 5000 at:`, and your IPs | |
| A2 | Check the build folder | `hearts.db` and `logs/server-<today>.log` exist | |
| A3 | Connect with `send.py` | Server logs `connected     127.0.0.1:<port>` | |
| A4 | Connect from another computer on the same Wi-Fi, using an IP from A1 | Connects, server logs that machine's IP | |
| A5 | Close a `send.py` (Ctrl+C) | Server logs `disconnected  ...  (N messages in history)` | |
| A6 | Start a second server on the same port while one is running | Second one logs `listen failed: ...` and exits | |
| A7 | Run `test_server abc` | Logs `invalid port: abc` and exits | |

## B. Message format and requestId

| ID | Send | Expected reply | Result |
|---|---|---|---|
| B1 | `hello` | `{"command":"error","message":"invalid JSON: ...","requestId":0}` | |
| B2 | `[1,2,3]` | `error`, `"message must be a JSON object"`, requestId 0 | |
| B3 | `{"command":"login","username":"a","password":"b"}` (no requestId) | `error` "needs a requestId...", requestId 0 | |
| B4 | `{"command":"login","requestId":0}` | Same as B3 | |
| B5 | `{"command":"login","requestId":-3}` | Same as B3 | |
| B6 | `{"command":"login","requestId":1.5}` | Same as B3 | |
| B7 | `{"command":"login","requestId":"7"}` (string id) | Same as B3 | |
| B8 | `{"command":"dance","requestId":8}` | `error` `unknown command: "dance"`, **requestId 8** | |
| B9 | Paste two valid messages (two lines) at once | Both get replies, in order | |
| B10 | Any valid request | Reply has the **same requestId** you sent | |

## C. Register

| ID | Send | Expected | Result |
|---|---|---|---|
| C1 | `{"command":"register","requestId":1,"username":"alice","email":"alice@example.com","password":"pw","pin":"0042"}` | `register_result`, success true, `playerId` 1 | |
| C2 | Repeat C1 exactly | success false, `username already taken` | |
| C3 | Register with `"email"` missing | success false, `...are all required` | |
| C4 | Register with `"pin"` missing | success false, `...are all required` | |
| C5 | `"email":"not-an-email"` | success false, `email address doesn't look valid` | |
| C6 | `"pin":"12"` (too short) | success false, `pin must be a string of 4-8 digits` | |
| C7 | `"pin":"123456789"` (too long) | Same as C6 | |
| C8 | `"pin":"12a4"` | Same as C6 | |
| C9 | `"pin":1234` (number, not string) | success false, `...are all required` | |
| C10 | Register `bob` with `"email":"Bob@Example.COM"` | success true; in the DB, email is `bob@example.com` | |
| C11 | Check the DB after C1 | Row has a 64-char `password_hash`, 32-char `password_salt`, `pin_hash`, `pin_salt`. **No plain "pw" or "0042" anywhere** | |
| C12 | Register two users with the same password | Their `password_hash` values are **different** (different salts) | |
| C13 | After registering, send `{"command":"logout","requestId":9}` | `you are not logged in` (registering doesn't log you in) | |

## D. Login and logout

_Needs `alice` / `pw` from C1._

| ID | Send | Expected | Result |
|---|---|---|---|
| D1 | `{"command":"login","requestId":1,"username":"alice","password":"wrong"}` | success false, `wrong username or password` | |
| D2 | Login as `nobody` | **Same** message as D1 | |
| D3 | Login with an empty password | `username and password are required` | |
| D4 | Login as alice / pw | success true, `playerId`, `username` | |
| D5 | Check the DB after D4 | alice: `connected` = 1, `last_logged_in` = now | |
| D6 | Log in again on the same connection | `already logged in as alice` | |
| D7 | In a **second** `send.py`, log in as alice | `already logged in from another connection` | |
| D8 | First client: `{"command":"logout","requestId":5}` | success true, `logged out` | |
| D9 | Check the DB after D8 | `connected` = 0, `last_logged_out` = now | |
| D10 | Second client: log in as alice now | success true | |
| D11 | Log in, then close `send.py` without logging out | Server logs disconnect with `(alice)`; DB `connected` = 0, `last_logged_out` set | |
| D12 | Logged in as alice, a second client logs in as bob | Both succeed | |

## E. Password recovery

_Needs `alice` registered with email `alice@example.com` and pin `0042`._

| ID | Send | Expected | Result |
|---|---|---|---|
| E1 | `{"command":"recover_password","requestId":1,"username":"alice","email":"alice@example.com","pin":"9999","newPassword":"new"}` | success false, `those recovery details don't match our records` | |
| E2 | Correct pin, wrong email | Same message as E1 | |
| E3 | Unknown username | Same message as E1 | |
| E4 | Missing `newPassword` | `...are all required` | |
| E5 | Correct username, email, pin `"0042"` | success true, `password changed` | |
| E6 | Log in with the **old** password | `wrong username or password` | |
| E7 | Log in with the **new** password | success true | |
| E8 | Recovery with the email in different case (`ALICE@example.com`) | success true | |
| E9 | Check the DB after E5 | `password_hash` **and** `password_salt` both changed; pin columns unchanged | |

## F. Logging and history

| ID | Steps | Expected | Result |
|---|---|---|---|
| F1 | Do C1 and D4, then open `logs/server-<today>.log` | Lines like `>  #1 register {...}` and `<  #1 register_result {...}` | |
| F2 | Search the log for `pw` and `0042` | Shows only `"***"`; the real values never appear (unless you sent invalid JSON) | |
| F3 | Restart the server, check the log | New lines are **added** to the same file; old lines are still there | |
| F4 | Log in, then disconnect | Disconnect line shows the message count, e.g. `(6 messages in history)` | |
| F5 | Send more than 100 messages on one connection, then disconnect | Count shows **100** (history is capped) | |
| F6 | Terminal vs. file | Terminal shows `hh:mm:ss`; file shows `yyyy-MM-dd hh:mm:ss`; same content | |

## G. Server console

| ID | Steps | Expected | Result |
|---|---|---|---|
| G1 | No clients connected; type `hello` in the server window | `[System] No clients connected to receive broadcast.` | |
| G2 | Two clients connected; type `game starts soon` | Both receive `{"command":"server_message","requestId":-1,"text":"game starts soon"}` | |
| G3 | Broadcast again | requestId is now **-2** | |
| G4 | Broadcast with clients not logged in | They still receive it | |
| G5 | Log in alice, then type `exit` | Server shuts down; DB: alice `connected` = 0, `last_logged_out` set | |

## H. Database schema and persistence

| ID | Steps | Expected | Result |
|---|---|---|---|
| H1 | Register a user, stop the server, start it again, log in | Login works (data persisted) | |
| H2 | Kill the server while logged in (Stop button in Qt Creator), restart | Startup resets `connected` to 0 for everyone | |
| H3 | **With an old (pre-11-16) DB:** start the server | Logs `old players table (schema v0) renamed to players_old`; both tables exist | |
| H4 | After H3, check `PRAGMA user_version` | `2` | |
| H5 | Restart after H3 | No rename message the second time | |

---

## Failures

_Write down any failing test here: ID, date, what happened, and whether it's fixed._

| ID | Date | What happened | Status |
|---|---|---|---|
| | | | |

---

## Change log
- **2026-11-16:** First version. Sections A–H.
