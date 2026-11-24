# Hearts Message Protocol

_Matches the server as of 2026-11-23. This is the source text for the team's
"message formats" Google Doc. Paste these sections over the matching ones there._

---

## Default (every message)

- One JSON object per line, ending in a newline (`\n`).
- Every message has `command` and `requestId`. Other fields sit **next to them at
  the top level**, not inside a `data` object.
- It's `requestId` with a **lowercase d**, matching the Java client's `RequestManager`.
- Use straight quotes (`"`). Curly quotes (“ ”) aren't valid JSON.

```json
{"command": "login", "requestId": 1, "username": "alice", "password": "pw"}
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
  "password": "pw",
  "pin": "0042"
}
```
- `pin`: a **string** of 4–8 digits, used for password recovery.
- `email`: must look like `x@y.z`. The server stores it lowercase.
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
| `INVALID_EMAIL` | Email doesn't look like `x@y.z` |
| `INVALID_PIN` | Pin isn't a string of 4–8 digits |
| `INVALID_CREDENTIALS` | Username is already taken |
| `SERVER_ERROR` | Database problem |

> **Multiple errors:** the server checks in the order above and returns the **first** problem only.

---

## Login (`login`)

**Client**
```json
{"command": "login", "requestId": 2, "username": "alice", "password": "pw"}
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
  "newPassword": "newpw"
}
```
All four fields are required, and username, email, and pin must all match the account.
Email is compared case-insensitively.

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
| `INVALID_EMAIL` | register |
| `INVALID_PIN` | register |
| `INVALID_CREDENTIALS` | register (username taken), login |
| `ALREADY_LOGGED_IN` | login |
| `ACCOUNT_IN_USE` | login |
| `ACCOUNT_LOCKED` | login |
| `NOT_LOGGED_IN` | logout |
| `RECOVERY_FAILED` | recover_password |
| `SERVER_ERROR` | register, recover_password |
| `INVALID_JSON` | error |
| `MISSING_REQUEST_ID` | error |
| `UNKNOWN_COMMAND` | error |

Defined in `HeartsServer/ErrorCodes.h`.

---

## Not implemented yet

The lobby messages in the Google Doc (`start_lobby`, `join_lobby`, `get_lobbies`,
`player_joined`, `player_left`, `host_left`) aren't on the server yet. When they're built,
they should follow the **Default** rules above: flat fields, `requestId`, and `error`
codes in the `*_result` replies.
