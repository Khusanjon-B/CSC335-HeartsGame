# Hearts Server — Manual Test Cases

A running checklist of tests a person runs by hand to check the server.
**Add new tests whenever a server feature is added.** Don't delete old ones;
they're the regression suite.

_Last updated: 2026-11-25 (covers: play again/disconnects, scoring/game end/match history, trick play, change password/ping, dealing/passing, lobbies/game sessions, connections, protocol, error codes, register, login/logout,
lockout, password recovery, hashing, history, logging, console, monitoring GUI, DB schema)_

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

**Then log the run in [TEST_RESULTS.md](TEST_RESULTS.md)** (team notes 11/3 and 11/24): the date, the commit you tested,
who ran it, and the **actual output** for each test. This file is the *protocol* (what to do and what should happen);
TEST_RESULTS.md is the *record* of what actually happened.

---

## A. Startup and connections

| ID | Steps | Expected | Result |
|---|---|---|---|
| A1 | Start the server | Terminal shows the log-file path, `[DB] opened ...hearts.db`, `listening on port 5000 at:`, and your IPs. A **"Hearts Server Monitor"** window also opens | |
| A2 | Check the build folder | `hearts.db` and `logs/server-<today>.log` exist | |
| A3 | Connect with `send.py` | Server logs `connected     127.0.0.1:<port>` | |
| A4 | Connect from another computer on the same Wi-Fi, using an IP from A1 | Connects, server logs that machine's IP | |
| A5 | Close a `send.py` (Ctrl+C) | Server logs `disconnected  ...  (N messages in history)` | |
| A6 | Start a second server on the same port while one is running | Second one logs `listen failed: ...` and exits | |
| A7 | Run `test_server abc` | Logs `invalid port: abc` and exits | |

## B. Message format and requestId

| ID | Send | Expected reply | Result |
|---|---|---|---|
| B1 | `hello` | `{"command":"error","error":"INVALID_JSON","message":"invalid JSON: ...","requestId":0}` | |
| B2 | `[1,2,3]` | `error`, code `INVALID_JSON`, `"message must be a JSON object"`, requestId 0 | |
| B3 | `{"command":"login","username":"a","password":"b"}` (no requestId) | `error`, code `MISSING_REQUEST_ID`, requestId 0 | |
| B4 | `{"command":"login","requestId":0}` | Same as B3 | |
| B5 | `{"command":"login","requestId":-3}` | Same as B3 | |
| B6 | `{"command":"login","requestId":1.5}` | Same as B3 | |
| B7 | `{"command":"login","requestId":"7"}` (string id) | Same as B3 | |
| B8 | `{"command":"dance","requestId":8}` | `error`, code `UNKNOWN_COMMAND`, **requestId 8** | |
| B9 | Paste two valid messages (two lines) at once | Both get replies, in order | |
| B10 | Any valid request | Reply has the **same requestId** you sent | |
| B11 | Any request that succeeds | Reply has `"success":true` **and** `"error":""` | |
| B12 | Any request that fails | Reply has `"success":false` **and** a non-empty `"error"` code | |

## C. Register

| ID | Send | Expected | Result |
|---|---|---|---|
| C1 | `{"command":"register","requestId":1,"username":"alice","email":"alice@example.com","password":"Hearts#2026","pin":"0042"}` | `register_result`, success true, error `""`, `playerId` 1 | |
| C2 | Repeat C1 exactly | success false, `INVALID_CREDENTIALS`, `username already taken` | |
| C3 | Register with `"email"` missing | success false, `MISSING_FIELDS` | |
| C4 | Register with `"pin"` missing | success false, `MISSING_FIELDS` | |
| C5 | `"email":"not-an-email"` | success false, `INVALID_EMAIL` | |
| C6 | `"pin":"12"` (too short) | success false, `INVALID_PIN` | |
| C7 | `"pin":"123456789"` (too long) | Same as C6 | |
| C8 | `"pin":"12a4"` | Same as C6 | |
| C9 | `"pin":1234` (number, not string) | success false, `MISSING_FIELDS` | |
| C10 | Register `bob` with `"email":"Bob@Example.COM"` | success true; in the DB, email is `bob@example.com` | |
| C11 | Check the DB after C1 | Row has a 64-char `password_hash`, 32-char `password_salt`, `pin_hash`, `pin_salt`. **No plain "Hearts#2026" or "0042" anywhere** | |
| C12 | Register two users with the same password | Their `password_hash` values are **different** (different salts) | |
| C13 | After registering, send `{"command":"logout","requestId":9}` | `NOT_LOGGED_IN` (registering doesn't log you in) | |
| C14 | Register `"username":"bad name!"` | `INVALID_USERNAME` (TC 2.1) | |
| C15 | Register with `"password":"short1!"` (7 chars) | `INVALID_PASSWORD` (TC 2.1) | |
| C16 | Register with `"password":"nodigits!!"` | `INVALID_PASSWORD` | |
| C17 | Register with `"password":"nospecial123"` | `INVALID_PASSWORD` | |
| C18 | Register a new username with alice's email (any case, e.g. `ALICE@example.com`) | `INVALID_CREDENTIALS`, `email already in use` (TC 2.2) | |
| C19 | Bad username **and** a taken email together | Only the **first** problem is reported (`INVALID_USERNAME`) | |

## D. Login and logout

_Needs `alice` / `Hearts#2026` from C1._

| ID | Send | Expected | Result |
|---|---|---|---|
| D1 | `{"command":"login","requestId":1,"username":"alice","password":"wrong"}` | success false, `INVALID_CREDENTIALS` | |
| D2 | Login as `nobody` | **Same** code and message as D1 | |
| D3 | Login with an empty password | `MISSING_FIELDS` | |
| D4 | Login as alice / Hearts#2026 | success true, error `""`, `playerId`, `username` | |
| D5 | Check the DB after D4 | alice: `connected` = 1, `last_logged_in` = now | |
| D6 | Log in again on the same connection | `ALREADY_LOGGED_IN` | |
| D7 | In a **second** `send.py`, log in as alice | `ACCOUNT_IN_USE` | |
| D8 | First client: `{"command":"logout","requestId":5}` | success true, error `""`, `logged out` | |
| D9 | Check the DB after D8 | `connected` = 0, `last_logged_out` = now | |
| D10 | Second client: log in as alice now | success true | |
| D11 | Log in, then close `send.py` without logging out | Server logs disconnect with `(alice)`; DB `connected` = 0, `last_logged_out` set | |
| D12 | Logged in as alice, a second client logs in as bob | Both succeed | |
| D13 | Send `logout` twice in a row | 1st: success true. 2nd: success false, `NOT_LOGGED_IN` | |
| D14 | Log out, then log in **on the same connection** (no reconnect) | Login succeeds; the connection stayed open after logout | |
| D15 | Log out, then send a message that needs login (none exist yet; recheck when game commands are added) | Rejected with `NOT_LOGGED_IN` | |

## E. Password recovery

_Needs `alice` registered with email `alice@example.com` and pin `0042`._

| ID | Send | Expected | Result |
|---|---|---|---|
| E1 | `{"command":"recover_password","requestId":1,"username":"alice","email":"alice@example.com","pin":"9999","newPassword":"N3wHearts#26"}` | success false, `RECOVERY_FAILED` | |
| E2 | Correct pin, wrong email | Same code as E1 | |
| E3 | Unknown username | Same code as E1 | |
| E4 | Missing `newPassword` | `MISSING_FIELDS` | |
| E5 | Correct username, email, pin `"0042"` | success true, `password changed` | |
| E6 | Log in with the **old** password | `wrong username or password` | |
| E7 | Log in with the **new** password | success true | |
| E8 | Recovery with the email in different case (`ALICE@example.com`) | success true | |
| E9 | Check the DB after E5 | `password_hash` **and** `password_salt` both changed; pin columns unchanged | |
| E10 | Log alice in on one client; on another, send a correct recovery | `ACCOUNT_IN_USE` (no recovery while logged in, notes 10/27) | |
| E11 | Correct recovery with `"newPassword":"weak"` | `INVALID_PASSWORD`; the old password still works | |

## F. Logging and history

| ID | Steps | Expected | Result |
|---|---|---|---|
| F1 | Do C1 and D4, then open `logs/server-<today>.log` | Lines like `>  #1 register {...}` and `<  #1 register_result {...}` | |
| F2 | Search the log for `Hearts#2026` and `0042` | Shows only `"***"`; the real values never appear (unless you sent invalid JSON) | |
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
| H4 | After H3, check `PRAGMA user_version` | `4` | |
| H5 | Restart after H3 | No rename message the second time | |
| H6 | **With a v2 DB (made 11-16 to 11-22):** start the server | Logs `upgraded from schema v2 to v3`; **all existing accounts still log in**; `failed_attempts` and `locked_until` columns exist | |
| H7 | **With a v3 DB (made 11-23 to 11-24):** start the server | Logs `upgraded from schema v3 to v4 (match tables added)`; all accounts kept; `matches` and `match_players` exist; `PRAGMA user_version` is `4` | |

## I. Failed logins and lockout (issue #54 · use case 7 · test case 7)

_Register `zed` / `Right#2026` first. Tip: for I7, set `locked_until` to a past time in DB Browser instead of waiting 5 minutes._

| ID | Send / do | Expected | Result |
|---|---|---|---|
| I1 | Login zed / `wrong` | `INVALID_CREDENTIALS`, `"failed_attempts":1`, message says `1 of 3 attempts used` | |
| I2 | Login zed / `wrong` again | `failed_attempts` 2 | |
| I3 | Login zed / `wrong` a 3rd time | `ACCOUNT_LOCKED`, `failed_attempts` 3, `lockedUntil` ≈ now + 5 min; server log says `LOCKED for 5 minutes` | |
| I4 | Login zed / **`Right#2026`** while locked | Still `ACCOUNT_LOCKED` (the correct password doesn't help) | |
| I5 | Login `nobody` / `x` | `INVALID_CREDENTIALS` with **no** `failed_attempts` field | |
| I6 | Check the DB during the lock | zed: `failed_attempts` 3, `locked_until` set | |
| I7 | Wait 5 minutes (or see the tip), login zed / `wrong` | `INVALID_CREDENTIALS`, `failed_attempts` **1** (count restarted) | |
| I8 | Then login zed / `Right#2026` | Success; DB `failed_attempts` 0, `locked_until` NULL | |
| I9 | 2 wrong, then right, then 1 wrong | Last reply shows `failed_attempts` 1 (success reset the count) | |
| I10 | Lock zed, restart the server, try logging in | Still `ACCOUNT_LOCKED` (the lock is saved in the DB) | |

## J. Server Monitoring GUI (#65, #72, #73, #74 · use cases 7.2, 12–15 · test cases 8, 13–16)

_Use the "Hearts Server Monitor" window. Open 2–3 `send.py` clients._

| ID | Steps | Expected | Result |
|---|---|---|---|
| J1 | Start the server, no clients; click **View Connected User Count** | `Connected clients: 0` and "No accounts currently connected" (TC 16.1) | |
| J2 | Connect 2 clients (don't log in) | The list updates **by itself** to 2 entries, both `(not logged in)` (TC 16) | |
| J3 | Click **View Logged-In Users** with nobody logged in | `Logged-in users: 0` and "No users currently logged in" (TC 13.1) | |
| J4 | Log one client in as alice | The list updates by itself: `alice (127.0.0.1:...)` (TC 13) | |
| J5 | alice logs out, or closes `send.py` | alice disappears from Logged-In (and from Connected if she closed it) | |
| J6 | Click **View Registered User Count** | `Registered users: N` matches the number of rows in `players` (TC 14) | |
| J7 | Register a new account while on that view | The count goes up by 1 by itself | |
| J8 | No locked accounts; click **View Locked-Out Users** | "No accounts currently locked"; Unlock button greyed out (TC 15.1) | |
| J9 | Lock an account (3 wrong passwords) while on that view | It appears with `(unlocks in 4:59)`, counting down (TC 15) | |
| J10 | Select it, click **Unlock Selected Account** | It disappears; the status line says `Unlocked "zed". They can sign in again.`; the terminal logs `[Admin] unlocked account` (TC 8) | |
| J11 | After J10, log in as zed with the right password | Success | |
| J12 | Lock an account and leave the window on Locked-Out for 5 minutes | It drops off the list by itself when the lock expires | |
| J13 | Type in the **Broadcast** box and press Enter | Every client receives a `server_message`; the box clears | |
| J14 | Close the monitor window | The server shuts down cleanly; logged-in users are marked logged out in the DB | |
| J15 | Type `exit` in the terminal | The server and the window both close | |

## K. Config file (`server.ini`)

| ID | Steps | Expected | Result |
|---|---|---|---|
| K1 | Delete `server.ini` next to the exe, start the server | It's recreated with `port=5000`, `maxFailedLogins=3`, `lockoutMinutes=5`, `maxConcurrentGames=1`; the log shows `[Config] created ...` | |
| K2 | Set `maxFailedLogins=2`, restart, enter 2 wrong passwords | Locked after **2** | |
| K3 | Set `lockoutMinutes=1`, lock an account | `lockedUntil` ≈ now + 1 min | |
| K4 | Set `port=6000`, restart with no arguments | Listens on 6000; `test_server 5000` still overrides it | |
| K5 | Set `maxFailedLogins=abc` | Log shows `bad value ... using default 3` | |
| K6 | Use a `server.ini` with no `gameOverScore` line, start the server | The line `gameOverScore=100` is added to the file; the other values are left alone | |
| K7 | Set `gameOverScore=30`, restart, play | The game ends after the first round where someone reaches 30 | |

## L. Lobbies and game sessions (#64, #63 · use cases 8–11 · test cases 9–12)

_Open 5 `send.py` clients and log each one in (alice, bob, carol, dave, erin). Keep the monitor window on **View Hearts Games**._

| ID | Send / do | Expected | Result |
|---|---|---|---|
| L1 | Not logged in: `{"command":"start_lobby","requestId":1}` | `NOT_LOGGED_IN` | |
| L2 | alice: `start_lobby` | `lobbyId` 1, `playerNum` 1, `players:["alice"]`; the monitor shows `Lobby 1 [Waiting for players] 1/4` (TC 9) | |
| L3 | bob: `start_lobby` (`maxConcurrentGames=1`) | `COULD_NOT_CREATE_LOBBY`, "no additional games can be created at this time" (TC 12.1) | |
| L4 | bob: `get_lobbies` | `lobbies:[{"lobbyId":1,"host":"alice","playerCount":1}]` | |
| L5 | bob: `join_lobby` with `"lobbyId":99` | `LOBBY_NOT_FOUND` | |
| L6 | bob: `join_lobby` with `"lobbyId":1` | `playerNum` 2, `players:["alice","bob"]`, `host:"alice"`; alice gets `player_joined` (TC 10) | |
| L7 | bob: `join_lobby` 1 again | `ALREADY_IN_LOBBY` | |
| L8 | alice: `start_game` with 2/4 | `LOBBY_NOT_FULL` ("not enough players… (2/4)") | |
| L9 | bob: `start_game` | `NOT_HOST` | |
| L10 | carol and dave join | Each existing member gets `player_joined`; dave's `playerNum` is 4 | |
| L11 | erin: `join_lobby` 1 | `LOBBY_FULL` | |
| L12 | carol: `leave_lobby` | Success; the others get `player_left` with `players:["alice","bob","dave"]` (dave moved up); the lobby stays open (TC 12.2) | |
| L13 | carol: close `send.py` instead (after rejoining) | Same as L12: the others get `player_left` | |
| L14 | Lobby full again, alice: `start_game` | Success; **all 4** get `game_started` with `players`; the monitor shows `[In game]` (TC 12) | |
| L15 | erin: `join_lobby` 1 / `get_lobbies` | `GAME_ALREADY_STARTED` / `lobbies:[]` | |
| L16 | alice (host) sends `leave_lobby` | The others get `host_left` ("The host left, so the game was disbanded."); the lobby is gone from the monitor (TC 11) | |
| L17 | After L16, bob sends `leave_lobby` | `NOT_IN_LOBBY` (the server already removed him) | |
| L18 | Make a lobby, then the host **logs out** / **closes the client** | Same as L16 | |
| L19 | After a lobby is disbanded, erin: `start_lobby` | Success (the game slot is free again) | |
| L20 | Set `maxConcurrentGames=2`, restart; two hosts `start_lobby` | Both succeed; `get_lobbies` lists both | |

## M. Table setup, dealing, and passing (#71, #70 · use cases 16–17 · test cases 17–18)

_Get 4 logged-in clients into one lobby (section L) and have the host send `start_game`._

| ID | Send / do | Expected | Result |
|---|---|---|---|
| M1 | Host: `start_game` | All 4 get `game_started` with `players` in lobby order and `"scores":[0,0,0,0]` (TC 17) | |
| M2 | Look at each client's `round_started` | `round` 1, `passDirection` `"left"`, 13 cards, sorted C→D→S→H, low→high (TC 18) | |
| M3 | Compare all 4 hands | 52 different cards, no duplicates | |
| M4 | Check `passTo` | Each player's `passTo` is the **next** player in `players` (the last one passes to the first) | |
| M5 | Monitor → View Hearts Games | `In game - round 1, passing left` | |
| M6 | `pass_cards` with only 2 cards | `INVALID_CARDS`, "pass exactly 3 cards" | |
| M7 | `pass_cards` with a card that isn't in your hand | `INVALID_CARDS`, "you can only pass cards from your hand" | |
| M8 | `pass_cards` with the same card twice | `INVALID_CARDS`, "you picked the same card twice" | |
| M9 | `pass_cards` with `"1X"` | `INVALID_CARDS`, `"1X" isn't a card…` | |
| M10 | Valid `pass_cards` (3 of your cards, e.g. `["QD","10S","JH"]`) | Success, "waiting for the other players"; the other 3 get `player_passed` with your name | |
| M11 | Same player passes again | `ALREADY_PASSED` | |
| M12 | No one else has passed yet | Nobody gets `cards_received` or `player_turn` yet | |
| M13 | The other 3 pass | Each gets `cards_received`: `from` = the player before them, `cards` = exactly what that player passed, `hand` = 13 cards without the ones they passed | |
| M14 | Right after M13 | All 4 get `player_turn` naming the player whose new `hand` has `"2C"`, with their `playerNum`, `trick` 1 and `"leading":true`; only that player's copy has `"legalCards":["2C"]` (TC 18) | |
| M15 | Anyone sends `pass_cards` after M14 | `NOT_PASSING_PHASE` | |
| M16 | A player who isn't in a game sends `pass_cards` | `NOT_IN_GAME` | |
| M17 | Monitor after M14 | `In game - round 1, trick 1` | |

_Rounds 2–4 (right, across, hold) can't be reached until scoring (#69) is built. The rotation was checked with a direct test of `Table`._

## N. Change password and ping (#86, #89 · use cases 4, 22 · test cases 4–4.3, 23)

_One `send.py` client. Register `alice` with password `Hearts#2026`._

| ID | Send / do | Expected | Result |
|---|---|---|---|
| N1 | Not logged in: `{"command":"ping","requestId":1}` | `ping_result`, success, `"message":"pong"`, a `serverTime` (TC 23) | |
| N2 | Not logged in: `change_password` | `NOT_LOGGED_IN` | |
| N3 | Log in, then `{"command":"change_password","requestId":3,"newPassword":"N3wHearts#26"}` | `MISSING_FIELDS` | |
| N4 | `change_password` with `"currentPassword":"Wrong#2026"` | `INVALID_CREDENTIALS`, "your current password is incorrect" (TC 4.1) | |
| N5 | Right current password, `"newPassword":"short"` | `INVALID_PASSWORD` with the password rule (TC 4.2) | |
| N6 | Right current password, new = current | `SAME_PASSWORD` (TC 4.3) | |
| N7 | `{"command":"change_password","requestId":7,"currentPassword":"Hearts#2026","newPassword":"N3wHearts#26"}` | Success, "password changed"; still logged in (TC 4) | |
| N8 | `logout`, then log in with the **old** password | `INVALID_CREDENTIALS` | |
| N9 | Log in with `N3wHearts#26` | Success | |
| N10 | Look at the log | Both passwords show as `"***"` | |
| N11 | Stop the server, then send `ping` from the client | No reply (the client should show its connection error after 3 tries) | |

## O. Playing tricks (#66 · use case 18 · test cases 19, 19.1)

_Continue from section M (passing done, `player_turn` received). "Leader" = the player named in `player_turn`._

| ID | Send / do | Expected | Result |
|---|---|---|---|
| O1 | Before passing finishes, anyone: `{"command":"play_card","requestId":30,"card":"2C"}` | `NOT_YOUR_TURN`, "cards are still being passed" | |
| O2 | A player who isn't the leader plays a card | `NOT_YOUR_TURN` | |
| O3 | Leader plays `"1X"` | `INVALID_CARDS` | |
| O4 | Leader plays a card they don't have | `CARD_NOT_IN_HAND` | |
| O5 | Leader plays anything but `"2C"` | `MUST_LEAD_TWO_OF_CLUBS` (TC 19.1) | |
| O6 | Leader plays `"2C"` | Success with their new `hand` (12 cards); all 4 get `card_played` with `cardsOnTable` holding 1 card; the next seat gets `player_turn` with `legalCards` | |
| O7 | Next player has a club but plays another suit | `MUST_FOLLOW_SUIT` (TC 19.1) | |
| O8 | A player with **no clubs** on trick 1 tries a heart or `"QS"` (if they have another card) | `NO_POINTS_FIRST_TRICK` | |
| O9 | All 4 play | Everyone gets `trick_won`: the highest club wins, `points` 0 (no points allowed on trick 1); then `player_turn` names the winner with `"leading":true` (TC 19) | |
| O10 | Before any heart has been played, the leader leads a heart (and has other suits) | `HEARTS_NOT_BROKEN` | |
| O11 | Someone who can't follow suit plays a heart | Accepted; `card_played` has `"heartsBroken":true`; hearts can now be led | |
| O12 | A trick containing `"QS"` and 2 hearts | `trick_won` `points` = 15 | |
| O13 | Compare `legalCards` with the rules in each `player_turn` | They always match what the server accepts | |
| O14 | Play all 13 tricks | The points in all the `trick_won` messages add up to 26; every hand is empty | |
| O15 | Monitor during play | `In game - round 1, trick N` | |

## P. Scoring, game end, and match history (#69, #68, #87 · use cases 19, 20, 6 · test cases 20, 21, 6, 6.1)

_Tip: set `gameOverScore=30` in `server.ini` and restart, so a game ends after 1–3 rounds instead of ~10.
Keep a tally of each player's `trick_won` points per round._

| ID | Send / do | Expected | Result |
|---|---|---|---|
| P1 | Play all 13 tricks of a round | Everyone gets `round_ended`: `roundPoints` = each player's tally, `scores` = running totals, `shotTheMoon` `""` (TC 20) | |
| P2 | One player takes all 13 hearts and the Queen of Spades | `round_ended`: that player 0, the others 26 each; `shotTheMoon` = their name (TC 20) | |
| P3 | After a round where nobody reached the limit | A new `round_started` arrives right away: round 2 `right`, round 3 `across`, round 4 `hold` (no passing, `player_turn` comes straight away), round 5 `left` | |
| P4 | A round ends with someone at or above `gameOverScore` | Everyone gets `game_ended` with `players`, `scores`, `places`, `winners` (lowest score), `saved:true`, a `matchId` (TC 21) | |
| P5 | Two players tie for the lowest score at the end | Both are in `winners`, and both have place 1 | |
| P6 | After P4, anyone: `play_card` | `NOT_YOUR_TURN` | |
| P7 | Monitor after P4 | `Game over after N round(s)` | |
| P8 | DB after P4 | One `matches` row; 4 `match_players` rows with each seat, score, place | |
| P9 | A player from the game: `{"command":"get_match_history","requestId":50}` | `matches` has the game with their `score`, `place`, `won`, and all 4 `players`; `stats`: `gamesPlayed` 1, `wins` 1 or 0, `losses` the opposite, `winPercentage` 100 or 0 (TC 6) | |
| P10 | A new account: `get_match_history` | `matches:[]`, all stats 0, "No matches played yet." (TC 6.1) | |
| P11 | Not logged in: `get_match_history` | `NOT_LOGGED_IN` | |
| P12 | Right after `start_game`, in DB Browser rename table `matches` to `matches_x` and click **Write Changes**; play to the end | `game_ended` still arrives, with `saved:false`, `matchId` -1, "An error occurred while trying to reach the database."; log says `COULD NOT SAVE the match` (UC 20 exception). Rename it back afterwards: `get_match_history` shows nothing half-saved | |
| P13 | Restart the server, then `get_match_history` | The saved matches are still there | |

## Q. Play again, leaving, and disconnecting mid-game (#67, #88 · use cases 10, 18b, 21 · test cases 12.2, 19.2, 22, 22.1)

_4 clients: alice (host), bob, carol, dave. Use `gameOverScore=10` so games end quickly. "Close" = close that `send.py` window._

| ID | Send / do | Expected | Result |
|---|---|---|---|
| Q1 | Mid-game, bob: `{"command":"play_again","requestId":80}` | `GAME_NOT_OVER` | |
| Q2 | After `game_ended`, alice: `play_again` | Success, `votes:["alice"], needed:4`; the other 3 get `play_again_vote` | |
| Q3 | alice: `play_again` again | `ALREADY_VOTED` | |
| Q4 | bob, carol, dave: `play_again` | After the 4th vote, all 4 get `game_started` with the same `players` and `scores:[0,0,0,0]`, then `round_started` round 1 `left` (TC 22) | |
| Q5 | After the next `game_ended`, alice votes yes, then bob: `{"command":"play_again","requestId":85,"vote":false}` | bob: success "you left the game"; alice, carol and dave get `game_stopped` with `reason:"declined"`; everyone's `leave_lobby` now says `NOT_IN_LOBBY` (TC 22.1) | |
| Q6 | After a `game_ended`, carol: `leave_lobby` | The others get `game_stopped`, `reason:"player_left"`, "carol left, so there won't be a rematch." (TC 22.1) | |
| Q7 | Mid-game, carol: `leave_lobby` | The others get `game_stopped`, "carol left, so the game was stopped."; the monitor shows no games | |
| Q8 | Mid-game, nobody missing, alice: `disband_game` | `GAME_NOT_PAUSED` | |
| Q9 | During passing (alice has already passed), **close** carol | The others get `player_disconnected` (carol, `playerNum` 3); the monitor shows `Paused - waiting for carol` (TC 19.2) | |
| Q10 | bob: `pass_cards` | `GAME_PAUSED` | |
| Q11 | bob: `disband_game` | `NOT_HOST` | |
| Q12 | Open a new `send.py`, log in as carol | `login_result`, then `game_rejoined`: same `hand` as before, `phase:"passing"`, `hasPassed:false`, `passTo`, `waitingFor:[]`; the others get `player_reconnected` (TC 12.2) | |
| Q13 | alice: `pass_cards` again | `ALREADY_PASSED` (her pass from before the drop was kept); the other 3 pass → `cards_received` as normal | |
| Q14 | After the first card of a trick is down, **close** the player whose turn is next, then log them back in | `game_rejoined` has `phase:"playing"`, `cardsOnTable` with that card, `turn` = them; everyone gets `player_turn` again and theirs has `legalCards` | |
| Q15 | Finish that game | It plays to `game_ended` normally | |
| Q16 | New game; **close** dave; alice: `disband_game` | alice: success; bob and carol get `game_stopped`, `reason:"host_disbanded"` | |
| Q17 | Log in as dave afterwards | `login_result` only, no `game_rejoined`; dave is on the main menu | |
| Q18 | New game; **close** alice (the host) | The others get `host_left` (use case 10); the game is gone | |
| Q19 | New game; **close** bob AND carol; log bob back in | bob's `game_rejoined` has `waitingFor:["carol"]`; still `GAME_PAUSED` until carol logs in too | |

---

## Failures

_Write down any failing test here: ID, date, what happened, and whether it's fixed._

| ID | Date | What happened | Status |
|---|---|---|---|
| | | | |

---

## Change log
- **2026-11-25 (d):** Section Q (play again, leaving, disconnect/rejoin, disband; #67 and #88).
- **2026-11-25 (c):** Section P (scoring, game end, match history); H7 (v3→v4); H4 now expects 4; K6–K7 (`gameOverScore`); results now go in TEST_RESULTS.md.
- **2026-11-25 (b):** Section O (playing tricks, #66); M14 and M17 updated for `legalCards` and the trick number.
- **2026-11-25 (a):** Section N (change password and ping, #86 and #89).
- **2026-11-16:** First version. Sections A–H.
- **2026-11-19:** Added error codes to expected results; B11–B12 (error field); D13–D15 (logout, issue #59).
- **2026-11-23 (d):** Section M (table setup, dealing, and passing, #71 and #70).
- **2026-11-23 (c):** Section L (lobbies and game sessions, #64 and #63).
- **2026-11-23 (b):** Strong example passwords (the server now enforces the client's password rule); C14–C19 (username, password, unique email); E10–E11; section K (`server.ini`).
- **2026-11-23:** Added section I (lockout, #54) and section J (Server Monitoring GUI, #65/#72/#73/#74); H6 (v2→v3 upgrade); A1 mentions the GUI.
