# Hearts Message Protocol

_Matches the server as of 2026-11-23 (lobbies added). This is the source text for the team's
"message formats" Google Doc. Paste these sections over the matching ones there._

---

## Default (every message)

- One JSON object per line, ending in a newline (`\n`).
- Every message has `command` and `requestId`. Other fields sit **next to them at
  the top level**, not inside a `data` object.
- It's `requestId` with a **lowercase d**, matching the Java client's `RequestManager`.
- Use straight quotes (`"`). Curly quotes (“ ”) aren't valid JSON.

```json
{"command": "login", "requestId": 1, "username": "alice", "password": "Hearts#2026"}
```

### requestId
| Who sends | Value |
|---|---|
| Client request | Positive whole number, counting up (1, 2, 3, …) |
| Server reply | **Same** requestId as the request it answers |
| Server-initiated (e.g. `server_message`) | Negative (-1, -2, …), so it can never match a pending request |
| Server reply to an unreadable message | `0` |

### Every reply (`<command>_result`)
| Field | Type | Meaning |
|---|---|---|
| `command` | string | The request's command + `_result`, e.g. `login_result` |
| `requestId` | number | Copied from the request |
| `success` | bool | `true` or `false` |
| `error` | string | `""` on success, otherwise an **error code** (see the table at the end) |
| `message` | string | Human-readable text, for display or logs. **Don't branch on it**; use `error` |

Some replies add extra fields on success (listed per command).

---

## Create Account (`register`)

**Client**
```json
{
  "command": "register",
  "requestId": 1,
  "username": "alice",
  "email": "alice@example.com",
  "password": "Hearts#2026",
  "pin": "0042"
}
```
- `pin`: a **string** of 4–8 digits, used for password recovery.
- `username`: letters, digits, `_` and `-` only.
- `password`: at least 9 characters, with at least one letter, one digit, and one special character (the same rule as the client).
- `email`: must look like `x@y.z`. The server stores it lowercase. **One email = one account.**
- Registering does **not** log you in. Send `login` next.

**Server, success**
```json
{"command": "register_result", "requestId": 1, "success": true, "error": "", "message": "account created, you can now log in", "playerId": 1}
```

**Server, failure**
```json
{"command": "register_result", "requestId": 1, "success": false, "error": "INVALID_CREDENTIALS", "message": "username already taken"}
```
| `error` | When |
|---|---|
| `MISSING_FIELDS` | Any of username / email / password / pin is missing or blank |
| `INVALID_USERNAME` | Username has characters other than letters, digits, `_`, `-` |
| `INVALID_PASSWORD` | Password breaks the rule above |
| `INVALID_EMAIL` | Email doesn't look like `x@y.z` |
| `INVALID_PIN` | Pin isn't a string of 4–8 digits |
| `INVALID_CREDENTIALS` | Username already taken, **or** email already in use (`message` says which) |
| `SERVER_ERROR` | Database problem |

> **Multiple errors:** the server checks in the order above and returns the **first** problem only.

---

## Login (`login`)

**Client**
```json
{"command": "login", "requestId": 2, "username": "alice", "password": "Hearts#2026"}
```

**Server, success**
```json
{"command": "login_result", "requestId": 2, "success": true, "error": "", "message": "welcome alice", "playerId": 1, "username": "alice"}
```

**Server, failure**
```json
{"command": "login_result", "requestId": 2, "success": false, "error": "INVALID_CREDENTIALS", "message": "wrong username or password (1 of 3 attempts used)", "failed_attempts": 1}
```
| `error` | When | Extra fields |
|---|---|---|
| `MISSING_FIELDS` | Username or password missing or blank | |
| `INVALID_CREDENTIALS` | Unknown username **or** wrong password (deliberately the same code) | `failed_attempts`, only when the username exists |
| `ACCOUNT_LOCKED` | 3 wrong passwords in a row; locked for 5 minutes or until an admin unlocks it | `failed_attempts`, `lockedUntil` |
| `ALREADY_LOGGED_IN` | This connection is already logged in | |
| `ACCOUNT_IN_USE` | The account is logged in on another connection | |

**Server, locked**
```json
{"command": "login_result", "requestId": 4, "success": false, "error": "ACCOUNT_LOCKED", "message": "account locked after 3 failed attempts, try again in 5 minute(s)", "failed_attempts": 3, "lockedUntil": "2026-11-23T18:19:31"}
```

### Failed attempts and lockout (use case 7, issue #54)
- **`failed_attempts`** is the number of wrong passwords **in a row** for that account. It's sent on `INVALID_CREDENTIALS` (when the username exists) and on `ACCOUNT_LOCKED`. Treat it as optional: it's left out for unknown usernames.
- **The 3rd wrong password** locks the account for **5 minutes**. While locked, even the **right** password gets `ACCOUNT_LOCKED`.
- **`lockedUntil`** is the server's local time (ISO) when the lock ends. Clients can show "try again after …" or use the minutes in `message`.
- **A successful login** resets the count to 0. So does the lock running out.
- **An admin** can unlock the account early from the Server Monitoring GUI (use case 7.2).

> We use one code for "no such user" and "wrong password" (no `USERNAME_NOT_FOUND`)
> so the reply doesn't reveal which usernames exist.

---

## Logout (`logout`)

Ends the player's session. The connection **stays open**, so the client can return to the
login screen and log in again without reconnecting.

**Client**
```json
{"command": "logout", "requestId": 3}
```

**Server, success**
```json
{"command": "logout_result", "requestId": 3, "success": true, "error": "", "message": "logged out"}
```

**Server, failure**
```json
{"command": "logout_result", "requestId": 3, "success": false, "error": "NOT_LOGGED_IN", "message": "you are not logged in"}
```
| `error` | When |
|---|---|
| `NOT_LOGGED_IN` | This connection wasn't logged in |

> **Client note:** after **any** `logout_result`, success or not, the server treats the
> connection as logged out. Switch to the logged-out screen either way. If no reply arrives
> (connection lost), also show the logged-out screen; the server logs a dropped connection
> out automatically.

---

## Recover Password (`recover_password`)

**Client**
```json
{
  "command": "recover_password",
  "requestId": 4,
  "username": "alice",
  "email": "alice@example.com",
  "pin": "0042",
  "newPassword": "N3wHearts#26"
}
```
All four fields are required, and username, email, and pin must all match the account.
Email is compared case-insensitively. `newPassword` must follow the same rule as in `register`.
The account must **not** be logged in anywhere. After a reset, the player logs in again with the new password.

**Server, success**
```json
{"command": "recover_password_result", "requestId": 4, "success": true, "error": "", "message": "password changed, you can now log in"}
```

**Server, failure**
```json
{"command": "recover_password_result", "requestId": 4, "success": false, "error": "RECOVERY_FAILED", "message": "those recovery details don't match our records"}
```
| `error` | When |
|---|---|
| `MISSING_FIELDS` | Any of the four fields missing or blank |
| `RECOVERY_FAILED` | Username, email, or pin doesn't match (one shared code) |
| `ACCOUNT_IN_USE` | The account is logged in right now |
| `INVALID_PASSWORD` | `newPassword` breaks the password rule |
| `SERVER_ERROR` | Database problem |

---

## Error (`error`)

Sent by the server when a message is broken badly enough that no command can handle it.
There's no client-to-server `error` command.

```json
{"command": "error", "requestId": 0, "error": "INVALID_JSON", "message": "invalid JSON: illegal value"}
```
| `error` | When | `requestId` |
|---|---|---|
| `INVALID_JSON` | Not valid JSON, or not a JSON object | `0` |
| `MISSING_REQUEST_ID` | requestId missing, not a number, ≤ 0, or has decimals | `0` |
| `UNKNOWN_COMMAND` | No such command | The request's id |

---

## Server message (`server_message`)

Sent by the server on its own, for example when the operator types into the server console.

```json
{"command": "server_message", "requestId": -1, "text": "game starts soon"}
```

---

## All error codes

| Code | Used by |
|---|---|
| `""` (empty) | Every successful `*_result` |
| `MISSING_FIELDS` | register, login, recover_password |
| `INVALID_USERNAME` | register |
| `INVALID_PASSWORD` | register, recover_password |
| `INVALID_EMAIL` | register |
| `INVALID_PIN` | register |
| `INVALID_CREDENTIALS` | register (username or email taken), login |
| `ALREADY_LOGGED_IN` | login |
| `ACCOUNT_IN_USE` | login, recover_password |
| `ACCOUNT_LOCKED` | login |
| `NOT_LOGGED_IN` | logout, all lobby commands |
| `ALREADY_IN_LOBBY` | start_lobby, join_lobby |
| `COULD_NOT_CREATE_LOBBY` | start_lobby |
| `LOBBY_NOT_FOUND` | join_lobby |
| `LOBBY_FULL` | join_lobby |
| `GAME_ALREADY_STARTED` | join_lobby, start_game |
| `NOT_IN_LOBBY` | leave_lobby, start_game |
| `NOT_HOST` | start_game |
| `LOBBY_NOT_FULL` | start_game |
| `NOT_IN_GAME` | pass_cards |
| `NOT_PASSING_PHASE` | pass_cards |
| `ALREADY_PASSED` | pass_cards |
| `INVALID_CARDS` | pass_cards |
| `RECOVERY_FAILED` | recover_password |
| `SERVER_ERROR` | register, recover_password |
| `INVALID_JSON` | error |
| `MISSING_REQUEST_ID` | error |
| `UNKNOWN_COMMAND` | error |

Defined in `HeartsServer/ErrorCodes.h`.

---

## Lobbies and game sessions (use cases 8–11, issues #64 and #63)

You must be **logged in** for every command here. If you're not, the reply is `NOT_LOGGED_IN`.

**How seats work:** `players` is the list of usernames **in seat order**. `players[0]` is always the **host**.
Your `playerNum` is your position + 1, so the host is always `playerNum` 1. When someone leaves,
the players after them move up a seat. The server sends the new `players` list every time it changes,
so clients just redraw from it.

**Limits:** a lobby holds 4 players. Only `maxConcurrentGames` lobbies/games can exist at once
(`server.ini`, default **1**).

### Start a lobby (`start_lobby`)
```json
{"command": "start_lobby", "requestId": 5}
```
```json
{"command": "start_lobby_result", "requestId": 5, "success": true, "error": "", "message": "lobby created, waiting for players", "lobbyId": 1, "playerNum": 1, "host": "alice", "players": ["alice"]}
```
| `error` | When |
|---|---|
| `ALREADY_IN_LOBBY` | You're already in a lobby |
| `COULD_NOT_CREATE_LOBBY` | The game limit is reached ("no additional games can be created at this time") |

### List open lobbies (`get_lobbies`)
```json
{"command": "get_lobbies", "requestId": 6}
```
```json
{"command": "get_lobbies_result", "requestId": 6, "success": true, "error": "", "message": "1 open lobby(s)", "lobbies": [{"lobbyId": 1, "host": "alice", "playerCount": 3}]}
```
- **What's listed:** lobbies still waiting for players. Full ones are included, with `playerCount` 4. Started games aren't.
- **Nothing open:** `lobbies` is `[]`.

### Join a lobby (`join_lobby`)
```json
{"command": "join_lobby", "requestId": 7, "lobbyId": 1}
```
```json
{"command": "join_lobby_result", "requestId": 7, "success": true, "error": "", "message": "joined lobby", "lobbyId": 1, "playerNum": 2, "host": "alice", "players": ["alice", "bob"]}
```
| `error` | When |
|---|---|
| `MISSING_FIELDS` | No `lobbyId`, or it isn't a number |
| `LOBBY_NOT_FOUND` | No lobby with that id |
| `LOBBY_FULL` | Already 4 players |
| `GAME_ALREADY_STARTED` | That game started; the lobby is closed |
| `ALREADY_IN_LOBBY` | You're already in a lobby (leave it first) |

### Leave a lobby (`leave_lobby`)
```json
{"command": "leave_lobby", "requestId": 8}
```
```json
{"command": "leave_lobby_result", "requestId": 8, "success": true, "error": "", "message": "left the lobby"}
```
| `error` | When |
|---|---|
| `NOT_IN_LOBBY` | You're not in a lobby |

- **Disconnects and logouts count as leaving.** **Logging out** or **disconnecting** leaves your lobby the same way.
- **If the host leaves,** in any of those ways, the lobby is **disbanded** and everyone else gets `host_left` (use case 10).

### Start the game (`start_game`), host only
```json
{"command": "start_game", "requestId": 9}
```
```json
{"command": "start_game_result", "requestId": 9, "success": true, "error": "", "message": "game started"}
```
| `error` | When |
|---|---|
| `NOT_IN_LOBBY` | You're not in a lobby |
| `NOT_HOST` | Only the host can start |
| `LOBBY_NOT_FULL` | Fewer than 4 players ("not enough players to start the game (3/4)") |
| `GAME_ALREADY_STARTED` | It's already running |

Once a game starts, its lobby closes. It disappears from `get_lobbies`, and `join_lobby` gets `GAME_ALREADY_STARTED`.

### Events the server sends (negative `requestId`)
| `command` | Sent to | Fields |
|---|---|---|
| `player_joined` | Everyone already in the lobby | `lobbyId`, `username` (who joined), `players` |
| `player_left` | Everyone still in the lobby | `lobbyId`, `username` (who left), `players` |
| `host_left` | Everyone still in the lobby, which **no longer exists** | `lobbyId`, `username` (the host), `message` |
| `game_started` | Everyone in the lobby, host included | `lobbyId`, `players` (seat order), `scores` (`[0,0,0,0]`) |

```json
{"command": "player_joined", "requestId": -3, "lobbyId": 1, "username": "dave", "players": ["alice", "bob", "carol", "dave"]}
{"command": "host_left", "requestId": -8, "lobbyId": 1, "username": "alice", "message": "The host left, so the game was disbanded."}
```
**Client note for `host_left`:** stop the game, show the "host left" popup, and go back to the home screen.
The server has already taken you out of the lobby.

> If a player disconnects **during** a game, the others get `player_left`. Test case 19.2
> (wait for them to come back) belongs to the gameplay issues.

---

## Playing: table setup, dealing, and passing (use cases 16–17, issues #71 and #70)

### Card codes
Every card is a short string: **rank + suit letter**.

| Ranks | `2` `3` `4` `5` `6` `7` `8` `9` `10` `J` `Q` `K` `A` |
|---|---|
| **Suits** | `C` Clubs · `D` Diamonds · `S` Spades · `H` Hearts |

Examples: `"2C"` (2 of Clubs), `"10H"`, `"QS"` (Queen of Spades), `"AH"`. The server reads them case-insensitively.
Hands are sent **sorted**: Clubs, Diamonds, Spades, Hearts, each low to high.

### Seats
Seats go around the table in `players` order: index 0 → 1 → 2 → 3 → back to 0.
**Left** means the next seat, **right** the previous one, and **across** the opposite one.

### Order of messages for one round
```
everyone   <- game_started   {players, scores:[0,0,0,0]}          (first round only, #71)
each seat  <- round_started  {round, passDirection, passTo, hand}   private, 13 cards
each seat  -> pass_cards     {cards: [3 codes]}
others     <- player_passed  {username}
     ... once all 4 have passed ...
each seat  <- cards_received {from, cards, hand}                    private
everyone   <- player_turn    {username, playerNum, trick: 1}        2 of Clubs holder leads
```
On a **hold** round, `round_started` has `"passDirection":"hold"` and `"passTo":""`, and `player_turn` comes straight after.

**Pass direction by round:** 1 left, 2 right, 3 across, 4 hold, 5 left, and so on.

### `round_started` (server → one player)
```json
{"command": "round_started", "requestId": -7, "lobbyId": 1, "round": 1, "passDirection": "left", "passTo": "bob",
 "hand": ["7C", "JC", "3D", "6D", "QD", "2S", "5S", "8S", "10S", "2H", "3H", "4H", "JH"]}
```

### Pass cards (`pass_cards`)
```json
{"command": "pass_cards", "requestId": 12, "cards": ["QD", "10S", "JH"]}
```
```json
{"command": "pass_cards_result", "requestId": 12, "success": true, "error": "", "message": "cards chosen, waiting for the other players"}
```
The cards stay in your hand until **all four** players have chosen; then everyone's cards move at the same time.

| `error` | When |
|---|---|
| `NOT_IN_GAME` | You're not seated at a started game |
| `NOT_PASSING_PHASE` | Passing is over, or it's a hold round |
| `ALREADY_PASSED` | You already chose your 3 cards this round |
| `INVALID_CARDS` | Not exactly 3, a bad code, a card not in your hand, or the same card twice |

### `cards_received` (server → one player)
```json
{"command": "cards_received", "requestId": -9, "lobbyId": 1, "from": "dave", "cards": ["KC", "AD", "QH"],
 "hand": ["7C", "JC", "KC", "3D", "6D", "AD", "2S", "5S", "8S", "2H", "3H", "4H", "QH"]}
```
`hand` is your full new hand. Replace the old one with it.

### `player_turn` (server → everyone)
```json
{"command": "player_turn", "requestId": -10, "lobbyId": 1, "username": "dave", "playerNum": 4, "trick": 1}
```
If `username` is you, it's your turn. Right now this is only sent for the **first** trick (the 2 of Clubs
holder leads). Playing cards and the turns after that are issue #66.
