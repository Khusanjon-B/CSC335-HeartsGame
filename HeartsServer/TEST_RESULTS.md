# Hearts Server — Test Results

The **record** of test runs. [TESTING.md](TESTING.md) is the **protocol**: the steps and the expected result for each test.
This file keeps what actually happened when they were run (team notes 11/3: "actual output" column; 11/24: "Add test
protocols (results) while testing for final package").

**How to add a run:**
1. Copy the run template at the bottom.
2. Fill in the date, the commit you tested (`git log --oneline -1`), who ran it, and how.
3. For each test ID from TESTING.md, write the **actual output** (the key fields of the reply) and ✅ or ❌.
4. Copy any ❌ to the **Failures** table in TESTING.md.

**How the tests were run:**
- **Scripted:** small client scripts that connect to the server like real clients would. They send the
  messages from TESTING.md and compare the replies with the expected values. Multi-player tests use 4 scripted players.
  Server: `test_server 5055` with a fresh `hearts.db`.
- **Unit:** a small C++ program that calls the game classes directly, used for cases a random game can't reach
  on demand (Shoot the Moon, ties).
- **Manual:** a person using `send.py` and the monitor window.

---

## Summary

| Date | Commit | Area | Tests | Result |
|---|---|---|---|---|
| 2026-11-25 | `3d03b6d` | Change password, ping (#86, #89) | N1–N10 | ✅ 10 / 10 |
| 2026-11-25 | `e46fdfc` | Trick play (#66) + passing regression (#70, #71) | O1–O14, M1–M15 | ✅ all pass (3 full rounds) |
| 2026-11-25 | `051535f` | Scoring, game end, match history (#69, #68, #87) | P1–P13, H7, K6–K7 | ✅ all pass |

**Not yet run:** the GUI checks (J, M5, M17, O15, P7) were not re-run by hand for these changes. N11 isn't scripted. These need a manual pass.

---

## Run 2026-11-25 (a): change password and ping

- **Commit:** `3d03b6d` (branch `server-change-password-ping`)
- **Run by:** Claude, scripted (1 client)

| ID | Sent | Actual output | Result |
|---|---|---|---|
| N1 | `ping` (not logged in) | `success:true, message:"pong", serverTime:"2026-11-25T18:17:…"` | ✅ |
| N2 | `change_password` (not logged in) | `error:"NOT_LOGGED_IN"` | ✅ |
| N3 | `change_password` without `currentPassword` | `error:"MISSING_FIELDS"` | ✅ |
| N4 | current = `Wrong#2026` | `error:"INVALID_CREDENTIALS"` | ✅ |
| N5 | new = `short` | `error:"INVALID_PASSWORD"` | ✅ |
| N6 | new = current | `error:"SAME_PASSWORD"` | ✅ |
| N7 | `Hearts#2026` → `N3wHearts#26` | `success:true, message:"password changed"` | ✅ |
| N8 | Log out, log in with the old password | `error:"INVALID_CREDENTIALS"` | ✅ |
| N9 | Log in with the new password | `success:true` | ✅ |
| N10 | Search the log for the passwords | Not found in plain text; 5 `"currentPassword":"***"` entries | ✅ |

---

## Run 2026-11-25 (b): playing tricks

- **Commit:** `e46fdfc` (branch `server-tricks`)
- **Run by:** Claude, scripted (4 clients). Three separate full rounds with random legal plays. Before each legal
  play the script first tried an illegal card when one existed. The script's own copy of the Hearts rules checked
  every `legalCards` list and every trick winner.

| ID | Actual output | Result |
|---|---|---|
| O1 | `play_card` while passing → `error:"NOT_YOUR_TURN", message:"cards are still being passed"` | ✅ |
| O2 | Wrong player → `NOT_YOUR_TURN` | ✅ |
| O3 | `"1X"` → `INVALID_CARDS` | ✅ |
| O4 | Another player's card → `CARD_NOT_IN_HAND` | ✅ |
| O5 | Leading anything but 2C → `MUST_LEAD_TWO_OF_CLUBS` | ✅ |
| O6 | `2C` → success, `hand` has 12 cards; all 4 got `card_played` with 1 card in `cardsOnTable` | ✅ |
| O7 | `MUST_FOLLOW_SUIT` came up in all 3 rounds | ✅ |
| O8 | `NO_POINTS_FIRST_TRICK` came up in round 2 (a random deal only sometimes allows it) | ✅ |
| O9, O14 | 39 tricks (3 rounds × 13): every `trick_won` matched the script's winner and points | ✅ |
| O10 | `HEARTS_NOT_BROKEN` came up in all 3 rounds | ✅ |
| O11 | After a heart was played, `card_played.heartsBroken:true` and hearts could be led | ✅ |
| O13 | 156 turns: `legalCards` always matched the script's rules; only the active player's copy had `legalCards`; trick 1 = `["2C"]` | ✅ |
| O14 | `trick_won` points added up to 26 in each round; every hand was empty at the end | ✅ |
| — | `play_card` after trick 13 → refused | ✅ |
| M1–M15 | Passing test re-run after the `GameState` change: same results as 11-23 (13 cards each, 52 unique, `passTo` order, `cards_received` from the right player, `player_turn` to the 2C holder) | ✅ |

---

## Run 2026-11-25 (c): scoring, game end, match history

- **Commit:** `051535f` (branch `server-scoring-game-end`)
- **Run by:** Claude, scripted (4 clients) + unit (C++)

**Game 1:** `gameOverScore=30`.
- **Rounds:** 2.
- **Round totals:** after round 1 `3/0/4/19`; after round 2 `6/3/5/38`.
- **Result:** bob won and the match was saved as #1.

**Game 2:** default `gameOverScore=100`.
- **Rounds:** 12. The pass direction went `left,right,across,hold` three times over.
- **Final:** `[85, 64, 100, 63]`, places `[3, 2, 4, 1]`, dave won.

| ID | Actual output | Result |
|---|---|---|
| P1 | Every `round_ended` (2 + 12 rounds) matched the script's tally of `trick_won` points; `shotTheMoon:""` | ✅ |
| P2 | **Unit:** a rigged deal where seat 0 wins every trick. Seat 0 took 26 → `updateScore` gave `0/26/26/26`, `moonShooter = 0`. QS wasn't legal on trick 1; an all-hearts hand could play a heart on trick 1 | ✅ |
| P3 | Directions in game 2: `left,right,across,hold` ×3; on hold rounds `player_turn` came straight after `round_started` | ✅ |
| P4 | `game_ended`: `scores [85,64,100,63], places [3,2,4,1], winners ["dave"], rounds 12, matchId 1, saved true` | ✅ |
| P5 | **Unit:** scores `50/30/30/101` → places `3,1,1,4` (two winners). Game over at 100 and at 101, not at 99 | ✅ |
| P6 | `play_card` after the game → `NOT_YOUR_TURN` | ✅ |
| P8 | The match is in the DB: `matchId 1` read back through history with all 4 players | ✅ |
| P9 | Game 1: `alice {gamesPlayed:1, wins:0, losses:1, winPercentage:0}`, `bob {…wins:1, losses:0, winPercentage:100}`; each match's `score`/`won` matched `game_ended` | ✅ |
| P10 | New account → `matches:[]`, `"No matches played yet."`, all stats 0 | ✅ |
| P11 | Not logged in → `NOT_LOGGED_IN` | ✅ |
| P12 | `matches` table renamed mid-game → `game_ended {saved:false, matchId:-1, message:"An error occurred while trying to reach the database."}`; log `[DB] saveMatch failed: no such table: matches` and `COULD NOT SAVE the match`; after renaming it back, history was empty (nothing half-saved) | ✅ |
| P13 | Not run separately. Persistence works the same as H1 | — |
| H7 | **Copy** of the real `HeartsServer/build/hearts.db` (v3, 7 accounts) → log `upgraded from schema v3 to v4 (match tables added)`; afterwards `user_version 4`, 7 accounts, tables `players, players_old, matches, match_players` | ✅ |
| K6 | `server.ini` with only `[games] gameOverScore=30` → the missing `port`, `maxFailedLogins`, `lockoutMinutes` lines were added, and 30 was kept | ✅ |
| K7 | `gameOverScore=30` → the game ended after round 2 (38 ≥ 30) | ✅ |

---

## Run template

```
## Run YYYY-MM-DD: <area>

- **Commit:** <hash> (branch <name>)
- **Run by:** <name>, <manual / scripted / unit>

| ID | Actual output | Result |
|---|---|---|
| X1 | ... | ✅ / ❌ |
```
