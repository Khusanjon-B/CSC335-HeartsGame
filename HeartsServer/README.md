# Hearts Server — Current State

_Last updated: 2026-11-25_

A Qt 6 app that accepts TCP connections from Hearts clients, handles
accounts (register / login / logout / change password / password recovery / lockout), and stores players in
a SQLite database. The terminal shows the log, and a **Server Monitoring GUI** window lets
the admin see who's connected, who's logged in, the registered count, and locked
accounts, and unlock them. Players can create, list, join, and leave lobbies, and the host can start a
4-player game; the server then seats everyone, shuffles, deals, runs the 3-card pass, and checks every card played
against the Hearts rules and decides who wins each trick. Rounds are scored (with Shoot the Moon) until someone
reaches 100; then the match is saved and players can look up their match history and win/loss stats. All four can
vote to play again. A player who drops out mid-game can log back in and carry on.

**Message protocol:** [PROTOCOL.md](PROTOCOL.md) · **Manual test checklist:** [TESTING.md](TESTING.md) · **Test results:** [TEST_RESULTS.md](TEST_RESULTS.md)

---

## Status at a glance

| Feature | Status |
|---|---|
| TCP server (multiple clients) | ✅ Working |
| JSON message protocol with `requestId` | ✅ Working |
| Register (username, email, password, pin) | ✅ Working (tested) |
| Login / logout (issue #59) | ✅ Working (tested) |
| `error` code in every reply | ✅ Working (tested) |
| Failed-login tracking + 5-minute lockout after 3 tries (issue #54) | ✅ Working (tested) |
| Username / password rules, one email per account | ✅ Working (tested) |
| `server.ini` config file | ✅ Working (tested) |
| Password recovery with a pin | ✅ Working (tested) |
| Change password while logged in (#86, UC 4) | ✅ Working (tested) |
| `ping` connection check (#89, UC 22) | ✅ Working (tested) |
| Salted SHA-256 for passwords and pins | ✅ Working |
| Per-connection message history | ✅ Working (last 100 messages) |
| Log file + terminal logging | ✅ Working |
| Console broadcast + `exit` | ✅ Working |
| Server Monitoring GUI window | ✅ Working (tested) |
| ↳ View logged-in users (#65) | ✅ |
| ↳ View connected user count (#72) | ✅ |
| ↳ View locked-out users + unlock (#73, UC 7.2) | ✅ |
| ↳ View registered user count (#74) | ✅ |
| ↳ View Hearts Games (lobbies and running games) | ✅ |
| Game sessions: start / list / join / leave lobby, host starts game (#64) | ✅ Working (tested) |
| Host leaving disbands the lobby (#63) | ✅ Working (tested) |
| Max concurrent games (`server.ini`, default 1) | ✅ Working (tested) |
| Table setup: seats + scores 0 (#71) | ✅ Working (tested) |
| Shuffle, deal 13 each, 3-card pass (left / right / across / hold) (#70) | ✅ Working (tested) |
| 2 of Clubs holder prompted to lead trick 1 (#70) | ✅ Working (tested) |
| Card play with the Hearts rules, trick winner (#66) | ✅ Working (tested) |
| Round scoring + Shoot the Moon, next round or game end at 100 (#69) | ✅ Working (tested) |
| Game end: final results, match saved to the DB (#68) | ✅ Working (tested) |
| Match history + win/loss stats (`get_match_history`, #87, UC 6) | ✅ Working (tested) |
| Play again voting (#67, UC 21) | ✅ Working (tested) |
| Disconnect mid-game pauses; rejoin on login; host can disband (#88) | ✅ Working (tested) |
| Leaving a started game stops it for everyone (#88) | ✅ Working (tested) |

---

## Building and running

- **Qt:** 6.11.1, MinGW 64-bit (needs the `Core`, `Network`, `Sql`, and `Widgets` modules)
- **Build:** open `CMakeLists.txt` in Qt Creator and run it
- **Executable:** `test_server [port]`. The port comes from `server.ini` (default **5000**); one on the command line overrides it.
- **Files the server creates next to the `.exe`** (inside `build/`, which git ignores):
  - `hearts.db`: the database. Delete it to start over.
  - `server.ini`: settings (see [Config file](#config-file-serverini)). Created with defaults on first run.
  - `logs/server-YYYY-MM-DD.log`: one log file per day, appended to on each run.

Starting the server opens **two windows**:
- **The terminal:** the live log. You can still type here:
  - any text → broadcast to every client as a `server_message`
  - `exit` → logs everyone out in the DB and shuts down
- **"Hearts Server Monitor":** the admin GUI (see [Server Monitoring GUI](#server-monitoring-gui)).
  Closing it also shuts the server down.

---

## File layout

| File | What it does |
|---|---|
| `main.cpp` | Entry point. Opens the log file and the DB, starts the server, the monitor window and the console thread |
| `Server.h/.cpp` | TCP server, the `m_connections` list, JSON parsing, command handlers, sending, admin queries |
| `MonitorWindow.h/.cpp` | The Server Monitoring GUI window |
| `GameManager.h/.cpp` | One game session: lobbyId, its `Lobby`, state (waiting / in game), join and start rules |
| `Lobby.h` | The players in a lobby, in seat order (host first) |
| `Table.h/.cpp` | The game table: seats, scores, rounds, passing, and playing cards |
| `GameState.h/.cpp` | The cards in play in one round: hands, the current trick, whose turn, tricks taken, and the Hearts rules |
| `Card.h` | One card, the `"QS"` / `"10H"` card codes, and card points |
| `Deck.h` | 52-card deck: `newDeck()`, `shuffle()`, `dealCards()` |
| `Database.h/.cpp` | All SQL (players, matches). Every DB read and write goes through this class |
| `PasswordHasher.h/.cpp` | Salted SHA-256: `hash()` for new secrets, `verify()` for checking them |
| `Message.h` | `struct Message`, the requestId rules, and `redactSecrets()` |
| `ErrorCodes.h` | Every error code the server can send |
| `PROTOCOL.md` | The message protocol spec, written to match the team Google Doc |
| `TESTING.md` | Manual test protocol: steps and expected results, by feature |
| `TEST_RESULTS.md` | Dated test runs: what was tested, the actual output, pass/fail (team notes 11/3, 11/24) |
| `Connection.h` | `struct Connection`: one connected client, including its message history |
| `Log.h/.cpp` | `logMessage()` writes to the terminal and the log file |
| `Config.h/.cpp` | Reads `server.ini` (port, lockout limits, max games) |

**Comment conventions (issue #35):**
- **Every file** starts with a block saying what it's for and how it fits in.
- **Every function** is described where it's **declared** (the `.h`).
- **The `.cpp`** explains *how* it works, step by step where it isn't obvious.
- **Rules and limits** (pin length, lockout count, history size) are named constants at the top of the file that uses them, each with a comment.

---

## How it works

### Threads
- **Main thread:** runs Qt's event loop, which handles all networking, all database work, and the GUI window.
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
3. `start_lobby` / `join_lobby` → `lobbyId` is set. `leave_lobby` clears it (see [Game sessions](#game-sessions-and-lobbies-64-63--use-cases-811)).
4. `logout` → they leave any lobby, the login fields are cleared, and the DB row is marked disconnected. The socket stays open.
5. Socket disconnects → same as logout, then they're removed from `m_connections`.
   Exception: a non-host in a **running game** keeps their seat and the game pauses (#88). Their next `login` puts them back.

### `struct Connection`
| Field | Type | Meaning |
|---|---|---|
| `socket` | `QTcpSocket*` | The client's socket |
| `ip` | `QString` | `"ip:port"` |
| `playerId` | `std::optional<int>` | DB `player_id`, empty until login |
| `username` | `QString` | Empty until login |
| `verified` | `bool` | `true` once logged in |
| `lobbyId` | `std::optional<int>` | The lobby/game they're in; empty on the home screen |
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
| `register` | `username`, `email`, `password`, `pin` | `MISSING_FIELDS`, `INVALID_USERNAME`, `INVALID_PASSWORD`, `INVALID_EMAIL`, `INVALID_PIN`, `INVALID_CREDENTIALS`, `SERVER_ERROR` |
| `login` | `username`, `password` | `MISSING_FIELDS`, `INVALID_CREDENTIALS`, `ACCOUNT_LOCKED`, `ALREADY_LOGGED_IN`, `ACCOUNT_IN_USE` |
| `logout` | _(none)_ | `NOT_LOGGED_IN` |
| `recover_password` | `username`, `email`, `pin`, `newPassword` | `MISSING_FIELDS`, `RECOVERY_FAILED`, `ACCOUNT_IN_USE`, `INVALID_PASSWORD`, `SERVER_ERROR` |
| `change_password` | `currentPassword`, `newPassword` | `NOT_LOGGED_IN`, `MISSING_FIELDS`, `INVALID_CREDENTIALS`, `INVALID_PASSWORD`, `SAME_PASSWORD`, `SERVER_ERROR` |
| `ping` | _(none)_; works before login | — (reply has `serverTime`) |
| `start_lobby` | _(none)_ | `NOT_LOGGED_IN`, `ALREADY_IN_LOBBY`, `COULD_NOT_CREATE_LOBBY` |
| `get_lobbies` | _(none)_ | `NOT_LOGGED_IN` |
| `join_lobby` | `lobbyId` | `NOT_LOGGED_IN`, `MISSING_FIELDS`, `LOBBY_NOT_FOUND`, `LOBBY_FULL`, `GAME_ALREADY_STARTED`, `ALREADY_IN_LOBBY` |
| `leave_lobby` | _(none)_ | `NOT_LOGGED_IN`, `NOT_IN_LOBBY` |
| `start_game` | _(none)_ | `NOT_LOGGED_IN`, `NOT_IN_LOBBY`, `NOT_HOST`, `LOBBY_NOT_FULL`, `GAME_ALREADY_STARTED` |
| _(server)_ `error` | `error`, `message` | `INVALID_JSON`, `MISSING_REQUEST_ID`, `UNKNOWN_COMMAND` |
| _(server)_ `server_message` | `text` | — |
| _(server)_ `player_joined` / `player_left` | `lobbyId`, `username`, `players` | — |
| _(server)_ `host_left` | `lobbyId`, `username`, `message` | — |
| `pass_cards` | `cards` (3 codes) | `NOT_LOGGED_IN`, `NOT_IN_GAME`, `NOT_PASSING_PHASE`, `ALREADY_PASSED`, `INVALID_CARDS` |
| _(server)_ `game_started` | `lobbyId`, `players`, `scores` | — |
| _(server)_ `round_started` (private) | `lobbyId`, `round`, `passDirection`, `passTo`, `hand` | — |
| _(server)_ `player_passed` | `lobbyId`, `username` | — |
| _(server)_ `cards_received` (private) | `lobbyId`, `from`, `cards`, `hand` | — |
| _(server)_ `player_turn` | `lobbyId`, `username`, `playerNum`, `trick`, `leading`, `legalCards` (only for that player) | — |
| `play_card` | `card` | `NOT_LOGGED_IN`, `NOT_IN_GAME`, `INVALID_CARDS`, `NOT_YOUR_TURN`, `CARD_NOT_IN_HAND`, `MUST_LEAD_TWO_OF_CLUBS`, `MUST_FOLLOW_SUIT`, `NO_POINTS_FIRST_TRICK`, `HEARTS_NOT_BROKEN` |
| _(server)_ `card_played` | `lobbyId`, `username`, `playerNum`, `card`, `trick`, `cardsOnTable`, `heartsBroken` | — |
| _(server)_ `trick_won` | `lobbyId`, `username`, `playerNum`, `trick`, `cards`, `points` | — |
| _(server)_ `round_ended` | `lobbyId`, `round`, `roundPoints`, `scores`, `shotTheMoon` | — |
| _(server)_ `game_ended` | `lobbyId`, `players`, `scores`, `places`, `winners`, `rounds`, `matchId`, `saved`, `message` | — |
| `get_match_history` | _(none)_ | `NOT_LOGGED_IN`, `SERVER_ERROR` |
| `play_again` | `vote` (optional, default `true`) | `NOT_LOGGED_IN`, `NOT_IN_GAME`, `GAME_NOT_OVER`, `ALREADY_VOTED` |
| `disband_game` | _(none)_; host, while paused | `NOT_LOGGED_IN`, `NOT_IN_GAME`, `NOT_HOST`, `GAME_NOT_PAUSED` |
| _(server)_ `play_again_vote` | `lobbyId`, `username`, `votes`, `needed` | — |
| _(server)_ `player_disconnected` / `player_reconnected` | `lobbyId`, `username`, `playerNum` (+ `message`) | — |
| _(server)_ `game_rejoined` (private) | the whole table: `players`, `scores`, `round`, `phase`, `hand`, `cardsOnTable`, `turn`, ... | — |
| _(server)_ `game_stopped` | `lobbyId`, `username`, `reason` (`player_left` / `declined` / `host_disbanded`), `message` | — |
| `pass_cards`, `play_card` while paused | | `GAME_PAUSED` |

### Adding a new command
1. Declare `void handleXxx(Connection& conn, const Message& request);` in `Server.h`.
2. Implement it in `Server.cpp`: read from `request.body`, then call
   `replyOk(conn, request, "text", {extras})` or `replyFail(conn, request, ErrorCode::Xxx, "text")`.
3. Add `m_handlers["xxx"] = &Server::handleXxx;` in `registerHandlers()`.
4. Add any new error codes to `ErrorCodes.h`.
5. If it has a secret field, add the field name to `redactSecrets()` in `Message.h`.
6. Document it in `PROTOCOL.md`, and add tests to `TESTING.md`.

---

## Account rules (from the team notes and the client code)

| Rule | Source | Error code |
|---|---|---|
| Username: letters, digits, `_`, `-` only | Notes 9/29, client `validUsername()` | `INVALID_USERNAME` |
| Password: ≥ 9 characters, with a letter, a digit, and a special character | Client `validPassword()` | `INVALID_PASSWORD` |
| One email = one account | Notes 10/1, test case 2.2 | `INVALID_CREDENTIALS` ("email already in use") |
| One login per account at a time | Notes 10/27 | `ACCOUNT_IN_USE` |
| No password recovery while the account is logged in | Notes 10/27 | `ACCOUNT_IN_USE` |
| Change password: must know the current one, and the new one must differ | Use case 4, test cases 4.1–4.3 | `INVALID_CREDENTIALS`, `SAME_PASSWORD` |

The notes also mention a 15–128 character password rule. That conflicts with the client's
rule, so the server follows the client (it's the one already built). The limits are constants
at the top of `Server.cpp`.

---

## Config file (`server.ini`)

The team notes (9/29, 10/6) say the attempt limit and lock delay belong in an external file:

```ini
[server]
port=5000

[lockout]
maxFailedLogins=3
lockoutMinutes=5

[games]
maxConcurrentGames=1
gameOverScore=100
```
- **`gameOverScore`:** the game ends when someone reaches it. The notes say "User decides the ending score (default 100 points or more)".
  Lower it (e.g. `30`) to test game end quickly.
- **First run:** the file is created next to the exe with these defaults. Settings missing from an older file are added to it.
- **Applying changes:** edit the file and restart the server.
- **Bad values** fall back to the default, with a warning in the log.
- **The port** can still be overridden on the command line: `test_server 6000`.

---

## Failed logins and lockout (issue #54, use case 7)

- **Counting:** every wrong password adds 1 to the account's `failed_attempts`. The reply includes the count.
- **Locking:** on the **3rd** wrong password in a row, `locked_until` is set to now + **5 minutes**, and the reply is `ACCOUNT_LOCKED`.
- **While locked:** login is refused even with the right password.
- **Unlocking:** the lock ends on its own when time runs out. The next login attempt starts counting from 0. An admin can also unlock it early in the GUI.
- **Resetting:** a successful login resets the count to 0.
- **Unknown usernames** aren't counted, since there's no account to lock.
- **Changing the rules:** `maxFailedLogins` and `lockoutMinutes` are in `server.ini`.

---

## Server Monitoring GUI

`MonitorWindow` is the "Server Monitoring GUI" / "ServerGUI" from the use cases and class diagram.

| Button | Shows | Empty / error message | Use case · Issue |
|---|---|---|---|
| View Logged-In Users | Username and IP of each logged-in client | "No users currently logged in" | UC 12 · #65 |
| View Registered User Count | Number of accounts in the DB | "An error occurred while trying to reach the database." | UC 13 · #74 |
| View Locked-Out Users | Locked usernames and time until unlock, plus an **Unlock Selected Account** button | "No accounts currently locked" | UC 14 + 7.2 · #73 |
| View Hearts Games | Every lobby or running game: id, state, players, host | "No games currently running" | Class diagram `displayHeartsGames()` |
| View Connected User Count | Every open connection, logged in or not | "No accounts currently connected" | UC 15 · #72 |

- **Broadcast box:** works like typing in the terminal.
- **Staying current:** the window refreshes whenever the server emits `stateChanged()` (connect, disconnect, login, logout, register, lock, unlock), plus every 5 seconds so expired locks drop off.
- **Where the data comes from:** the window reads through `Server::viewLoggedInUsers()`, `viewConnectedUsers()`, `viewRegisteredUsers()`, `viewLockedAccounts()` and `unlockAccount()`, the same method names as the class diagram.
- **Threads:** the window runs on the main thread, so it needs no locks.

---

## Game sessions and lobbies (#64, #63 · use cases 8–11)

The server creates one **`GameManager`** per game (class diagram, notes 10/6). Each one holds a
**`Lobby`**: the players in seat order, host first.

```
start_lobby --> [Waiting for players] --(4 players + host sends start_game)--> [In game] <-------------+
                     |     ^                                                      |                    |
      join / leave --+-----+                                 someone reaches 100 --> [Finished] --(4 x play_again)
the host leaves (leave_lobby, logout, or disconnect), any state --> disbanded
```
- **Seats:** `players[0]` is the host (`playerNum` 1). The others are in join order. When someone leaves a lobby, the players after them move up.
- **Leaving:** `leave_lobby`, `logout`, and disconnecting all go through `Server::leaveLobby()`. A disconnect during a running game goes through `pauseForDisconnect()` first.
- **Host leaves:** everyone gets `host_left` and is taken out, and the `GameManager` is deleted (use case 10). This applies in every state.
- **Non-host leaves a lobby:** the lobby stays open, and the others get `player_left` with the new `players` (use case 11 extension).
- **Non-host leaves a started or finished game** (on purpose): Hearts needs 4 players, so everyone gets `game_stopped` and goes back to the menu (`Server::disbandGame()`).
- **Non-host disconnects mid-game** (#88, test case 19.2):
  - The game is **paused**. The player keeps their seat and cards, and the others get `player_disconnected`.
  - **They log back in:** they get `game_rejoined` with the whole table state, and play resumes.
  - **They don't come back:** the host can send `disband_game`.
- **Play again** (#67, use case 21): after `game_ended`, all 4 send `play_again` → `GameManager::playAgain()` gives a new `Table` (same seats, scores 0) and deals round 1.
  Any "no" vote, leave, or disconnect → `game_stopped` and everyone goes to the main menu (test case 22.1).
- **Starting:** only the host can start, and only with exactly 4 players. The lobby then closes to new players.
- **Game limit:** `maxConcurrentGames` in `server.ini` (default 1). A new `start_lobby` past the limit gets `COULD_NOT_CREATE_LOBBY`.
- **Starting** creates the game's `Table` (see the next section).

Full message formats are in [PROTOCOL.md](PROTOCOL.md#lobbies-and-game-sessions-use-cases-811-issues-64-and-63).

---

## The table: seats, dealing, and passing (#71, #70 · use cases 16–17)

When the host starts the game, `GameManager::startGame()` creates a **`Table`** (class diagram):
1. **Seats (use case 16):** the 4 players sit in lobby order (seat 0 = host), and every score is 0. Seats are copied from the lobby, so they never shift during the game.
2. **Round (use case 17):** `startNewRound()` makes a `Deck`, shuffles it on the server (so players can't tamper with it), and deals 13 cards to each player.
3. **Pass:** the direction rotates **left → right → across → hold** (team notes: "left right across hold - 4 cycles").
   - Each player picks 3 cards with `pass_cards`.
   - Nothing moves until all 4 have picked; then `applyPasses()` moves them all at once.
   - Hold rounds skip this step.
4. **Lead:** the holder of the **2 of Clubs** is sent `player_turn` to lead trick 1.

**Card format:** cards are codes like `"QS"`, `"10H"` (`Card::code()` / `Card::fromCode()`).

**Monitor:** the "View Hearts Games" view shows the round and phase, e.g. `In game - round 1, passing left`.

The `Table` never sends messages itself. `Server::announceRound()`, `handlePassCards()` and `announceTurn()` do that.

---

## Playing tricks (#66 · use case 18)

The cards in play live in a **`GameState`** (class diagram: `playerHands[][]`, `playerTricksTaken[][]`,
`activePlayer`, `playedCards[]`, `placeCard()`), owned by the `Table` and reset every round.

- **Turns:** `player_turn` goes to everyone. The player whose turn it is also gets `legalCards`, so the client can grey out the rest.
- **Checking a card:** `GameState::validateCard()` applies the rules in order:
  1. your turn and your card
  2. the 2 of Clubs leads trick 1
  3. follow suit
  4. no hearts or Queen of Spades on trick 1 (unless that's all you have)
  5. no leading hearts until they're broken (unless that's all you have)

  Each broken rule has its own error code, and the player just picks again (use case 18 extension a).
- **After each card:** everyone gets `card_played` with the whole trick so far (use case 18 step 2).
- **After the 4th card:** `determineTrickWinner()` finds the highest card of the led suit. Everyone gets `trick_won`, and the winner leads next.
- **After 13 tricks:** `Server::finishRound()` scores the round (#69).

`legalCards()` uses the same `validateCard()`, so what the client is offered and what the server accepts can never disagree.

---

## Scoring, game end, and match history (#69, #68, #87 · use cases 19, 20, 6)

- **Round scoring:** after trick 13, `Table::updateScore()` adds each player's points (hearts 1, Queen of Spades 13).
  - **Shoot the Moon:** a player who took all 26 points scores 0, and the others get 26.
  - Everyone gets `round_ended`.
- **Next round or end:**
  - **Nobody at `gameOverScore` (100):** the next round is dealt, and the pass direction keeps rotating.
  - **Somebody is:** `Server::endGame()` runs.
- **Game end:** the `GameManager` state becomes **Finished**. `Table::places()` ranks players by score (lowest = 1st; ties share a place). Then:
  - `Database::saveMatch()` writes the match in one transaction.
  - Everyone gets `game_ended` with the scores, places and winners.
  - If the DB write fails, `game_ended` still goes out, with `saved:false` and the use case 20 database error message.
- **Match history:** `get_match_history` returns the player's matches, newest first, plus stats (games, wins, losses, win %). Stats are counted from the match rows, not stored separately, so they can't drift.

The monitor shows `Game over after N round(s)` for a finished game.

---

## Passwords and pins

`PasswordHasher` stores `hash = SHA-256(salt + secret)` with a new random 16-byte salt for
every password and pin. Both are kept as hex text. Login, recovery and change password hash the input with
the stored salt and compare the results. Plain-text passwords and pins are never stored or logged.

This is a single SHA-256 pass: simple, and good enough for a class project.
(Production systems use deliberately slow hashes like bcrypt.)

---

## Database

SQLite through Qt's built-in `QSQLITE` driver, so no separate SQLite install is needed.

### Table `players` (schema version 3)
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
| `failed_attempts` | INTEGER | Wrong passwords in a row. Reset on success or unlock _(v3)_ |
| `locked_until` | TEXT | ISO time the lock ends; NULL or past means not locked _(v3)_ |

### Tables `matches` and `match_players` (schema version 4)
"Match DB" in the class diagram. One `matches` row per finished game, and one `match_players` row per player in it.

| Table | Column | Notes |
|---|---|---|
| `matches` | `match_id` | INTEGER PK AUTOINCREMENT |
| | `played_at` | ISO timestamp of the game end |
| | `rounds` | How many rounds were played |
| `match_players` | `match_id`, `player_id` | Primary key together |
| | `seat` | 0–3 |
| | `score` | Final score |
| | `place` | 1 = won (ties share a place) |

### Schema versions
The database file remembers its layout version in SQLite's `PRAGMA user_version`.
At startup, `Database::prepareSchema()` upgrades older files:
- **v0/v1** (plain-text passwords): the old table is **renamed to `players_old`** (kept, not deleted). Those accounts must register again.
- **v2 → v3:** the `failed_attempts` and `locked_until` columns are added. **All accounts are kept.**
- **v3 → v4:** the `matches` and `match_players` tables are added. Nothing else changes. **All accounts are kept.**

**To change the table:** edit the `CREATE TABLE`, bump `kSchemaVersion`, and add an upgrade step for the previous version (usually `ALTER TABLE ... ADD COLUMN`).

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
- [ ] No limit on wrong pin attempts. A 4-digit pin could be guessed by trying them all.
- [ ] No email verification (the use cases mark it "may not be implemented").
- [ ] `hearts.db` and the logs live in the build folder; deleting the build folder deletes them.
- [ ] A disconnected player who never comes back keeps the game paused until the host disbands it (no timeout yet; the notes mention a possible grace period)
- [ ] GUI checks (section J, M5, M17, O15, P7, Q-monitor) haven't been re-run by hand since 11-23; see TEST_RESULTS.md

---

## Change log

### 2026-11-25 (d) — Play again, leaving and disconnecting mid-game (#67, #88)
- **New command `play_again`** (`vote`, default true). When all 4 vote yes, a new game starts with the same players and scores 0; a "no" vote ends it for everyone. New event `play_again_vote`.
- **Disconnects:** a non-host dropping out of a running game **pauses** it (`player_disconnected`). Logging back in sends `game_rejoined` with the whole table, and play resumes (`player_reconnected`, then `player_turn`).
- **New command `disband_game`** (host, while paused).
- **Leaving on purpose:** a non-host leaving a started or finished game stops it for everyone (`game_stopped`, with a `reason`).
- **New error codes:** `GAME_PAUSED`, `GAME_NOT_PAUSED`, `GAME_NOT_OVER`, `ALREADY_VOTED`.
- **Monitor** shows `Paused - waiting for carol` and `… 2/4 voted to play again`.
- **Refactor:** the code that disbands a game is now shared (`Server::disbandGame()`).

### 2026-11-25 (c) — Scoring, game end, match history (#69, #68, #87)
- **Rounds are scored** with Shoot the Moon (`Table::updateScore()`), and everyone gets `round_ended`. Then the next round is dealt, or the game ends at `gameOverScore`.
- **Game end:** the new `GameManager` state is **Finished**. Everyone gets `game_ended` with scores, places and winners (lowest score wins, ties share).
- **DB schema v4:** new `matches` and `match_players` tables. `Database::saveMatch()` and `getMatchHistory()`. v3 files are upgraded in place.
- **New command `get_match_history`** with stats: games played, wins, losses, win %.
- **New `server.ini` setting `gameOverScore`** (default 100). Missing settings are now added to an existing `server.ini`.
- **New `TEST_RESULTS.md`:** dated test runs with actual outputs (team notes 11/3 and 11/24).

### 2026-11-25 (b) — Playing tricks (#66)
- **New class `GameState`** (class diagram). It holds the hands, the current trick, whose turn it is, tricks taken, and the Hearts rules. The `Table` owns one.
- **New command `play_card`.** New events `card_played` and `trick_won`.
- **`player_turn`** is now sent for every turn, with `leading`, and with `legalCards` for the player whose turn it is.
- **New error codes:** `NOT_YOUR_TURN`, `CARD_NOT_IN_HAND`, `MUST_LEAD_TWO_OF_CLUBS`, `MUST_FOLLOW_SUIT`, `NO_POINTS_FIRST_TRICK`, `HEARTS_NOT_BROKEN`.
- **Monitor** shows `In game - round 1, trick 5`.

### 2026-11-25 (a) — Change password + ping (#86, #89)
- **New command `change_password`** (`currentPassword`, `newPassword`) for logged-in players (use case 4). New error code `SAME_PASSWORD`.
- `currentPassword` is added to the redacted fields, so it never reaches the log.
- **New command `ping`** → `ping_result` with `serverTime` (use case 22). Works before login. The client retry rules are in PROTOCOL.md.

### 2026-11-23 (d) — Table setup, dealing, passing (#71, #70)
- **New classes:** `Card`, `Deck`, and `Table`, from the class diagram.
- **`game_started`** now includes `scores: [0,0,0,0]`. Seats follow the `players` order.
- **Round 1 is dealt automatically:** each player privately gets `round_started` with their 13-card hand and the pass direction.
- **New command `pass_cards`.** Once all 4 have passed, each player gets `cards_received`, then everyone gets `player_turn` for the 2 of Clubs holder.
- **Pass direction** rotates left → right → across → hold.
- **New error codes:** `NOT_IN_GAME`, `NOT_PASSING_PHASE`, `ALREADY_PASSED`, `INVALID_CARDS`.

### 2026-11-23 (c) — Game sessions and lobbies (#64, #63)
- **New classes:** `GameManager` (one per game) and `Lobby`, named after the class diagram.
- **New commands:** `start_lobby`, `get_lobbies`, `join_lobby`, `leave_lobby`, `start_game`.
- **New events:** `player_joined`, `player_left`, `host_left`, `game_started`.
- **Host leaving** (leave, logout, or disconnect) disbands the lobby. Anyone else leaving keeps it open.
- **Limits:** max concurrent games comes from `server.ini` (default 1).
- **Monitor window:** new "View Hearts Games" button.

### 2026-11-23 (b) — Rules from the team meeting notes
- **Validation:** usernames and passwords are now checked (`INVALID_USERNAME`, `INVALID_PASSWORD`), matching the client's rules.
- **One email = one account:** `register` rejects an email that's already in use.
- **Recovery:** refused while the account is logged in (`ACCOUNT_IN_USE`), and `newPassword` must follow the password rule.
- **New `server.ini` (`Config`):** port, `maxFailedLogins`, `lockoutMinutes`, `maxConcurrentGames`.

### 2026-11-23 — Lockout + Server Monitoring GUI (#54, #65, #72, #73, #74)
- 3 wrong passwords in a row lock an account for 5 minutes. `login_result` carries `failed_attempts`, plus `ACCOUNT_LOCKED` and `lockedUntil` when locked.
- New Server Monitoring GUI window (`MonitorWindow`): logged-in users, connected count, registered count, locked-out users, admin unlock, broadcast.
- `Server` emits `stateChanged()` and has the admin query methods from the class diagram.
- DB schema v3 adds `failed_attempts` and `locked_until`. v2 files are upgraded in place, keeping accounts.
- App uses `QApplication` (Qt Widgets) instead of `QCoreApplication`.

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
