# Limerino

A personal fork of 2547techno/technorino. MIT licensed, same as upstream.

## Upstream chain

| Repo | URL | Tracked branch | Role |
|---|---|---|---|
| Chatterino/chatterino2 | https://github.com/Chatterino/chatterino2 | `master` (remote `c2`) | Original upstream |
| SevenTV/chatterino7 | https://github.com/SevenTV/chatterino7 | `chatterino7` (remote `c7`) | 7TV fork of c2 |
| 2547techno/technorino | https://github.com/2547techno/technorino | `technorino` (remote `technorino`) | Direct upstream of this fork |
| lagx/Limerino | https://github.com/lagx/Limerino | `limerino` (remote `origin`) | This fork |

Standing fork delta at creation (from `technorino` @ f3bd14ea):
~1005 commits / 358 files over `c2/master`; technorino is ~345 commits / 116 files over `c7/chatterino7`. c2 → c7 → technorino merges are done by the upstream owners; we only follow technorino.

## Git topology

Remotes:

| remote | url | notes |
|---|---|---|
| `origin` | git@github.com:lagx/Limerino.git | push target; SSH (repo-local `core.sshCommand=C:/Windows/System32/OpenSSH/ssh.exe` — Git-for-Windows ssh cannot parse this machine's key) |
| `technorino` | https://github.com/2547techno/technorino.git | read-only upstream |
| `c7` | https://github.com/SevenTV/chatterino7.git | reference only, never merge directly |
| `c2` | https://github.com/Chatterino/chatterino2.git | reference only, never merge directly |

Branches:

| branch | tracks | rule |
|---|---|---|
| `limerino` | `origin/limerino` | Development branch; GitHub default branch. All fork work lands here. |
| `upstream-technorino` | `technorino/technorino` | Pristine mirror of upstream. **NEVER commit here.** |
| `technorino` | `technorino/technorino` | Leftover from the initial clone; harmless, kept (no branch deletions). |

Repo-local git config worth knowing (reapply on fresh clones): `rerere.enabled=true`,
`rerere.autoupdate=true`, `blame.ignoreRevsFile=.git-blame-ignore-revs`, `submodule.recurse=true`,
`core.sshCommand` as above.

## Merge policy

- Only ever merge `upstream-technorino` into `limerino`. Never merge c2 or c7 directly into
  `limerino` — the same commits arriving via two paths causes brutal repeat conflicts.
- Update cycle:

      git switch upstream-technorino && git pull --ff-only technorino technorino
      git switch limerino && git merge upstream-technorino
      git submodule update --init --recursive

  (`scripts/merge-upstream.sh` automates exactly this cycle; it refuses dirty trees and never
  auto-resolves conflicts.)
- Cherry-picks from c2/c7 are a last resort, always with `git cherry-pick -x`, and MUST be
  logged in the Cherry-picks table below (upstream SHA, repo, date, reason). When upstream later
  merges the same commit, resolve the conflict in favor of upstream and mark the row resolved.
- Cadence: merge upstream every 1–2 weeks. Small frequent merges beat quarterly big ones.
- Non-negotiables while merging: never `git push --force`, `git reset --hard`, `git clean -fdx`,
  `git rebase`, or delete branches; never run formatters/tidiers across untouched files.

## Modified upstream files

Every edit to an upstream file must add a row here **in the same commit**. Keep this count as
close to zero as possible — every entry is future merge pain.

| File | Why | Hook description | Risk on merge |
|---|---|---|---|
| `.gitignore` | ignore local `.ccache/` dir | appended 2-line "Limerino local dev" section at the end | Low — append-only; re-add if upstream rewrites the tail |
| `.gitattributes` | frozen auth script must survive checkouts byte-identically | one rule: `resources/limerino/limerinoauth.txt -text` (verbatim artifact; git LF normalization silently changed its bytes) | Low |
| `.github/workflows/build.yml` | CI must build the `limerino` branch; the `nightly-build` prerelease (which force-moves a tag) must point at `limerino`, not upstream | two edits: `limerino` added to `on.push.branches`; `create-release` job `if:` now `refs/heads/limerino` | **High — this file is updated upstream all the time. Expect a conflict on most merges. Resolution is always: keep upstream's version of the file, then re-apply these two exact edits.** |
| `.github/workflows/create-installer.yml` | installer workflow must fire after builds on `limerino` | one edit: `limerino` added to the `workflow_run.branches` filter | **High — same rule as build.yml: keep upstream's file, re-apply this edit** |
| `src/common/Version.cpp` | fork identity in window title/About; commit links must point at this repo | two string literals: `fullVersion_` `"Technorino "` → `"Limerino "`; commit URL host → `github.com/lagx/Limerino` | Low — narrow context, rarely touched upstream |
| `src/widgets/dialogs/SettingsDialog.cpp` | the fork needs its own settings tab | minimal hook: one `#include "limerino/LimerinoPage.hpp"` + one `addTab(...)` line (no id arg, icon `:/icon.png`); upstream Technorino tab label unchanged | Low |
| `src/controllers/commands/CommandController.hpp` | fork commands need a registration extension point | one tiny public passthrough `registerExternalCommand()` wrapping the private `registerCommand()` | Low |
| `src/controllers/commands/CommandController.cpp` | fork commands must be wired at startup | one include + one call `LimerinoCommands::initialize(*this)` at the tail of `initializeDefaults` + the passthrough impl | **Highest care — this file gets most upstream command additions; conflict resolution: keep upstream, re-add our 3 lines at the end of the function** |
| `src/widgets/dialogs/UserInfoPopup.cpp` | usercard gets a "Name history" option | one include + one `LabelButton` + connect (no `ui_` struct changes) | Low |
| `src/widgets/splits/SplitHeader.cpp` / `.hpp` | split menus + mod toolbar get fork actions | "Follow channel" + "View followers/following" menu lines (batch 2); predictions button member + creation + layout slot + vis hints in `updateIcons`/`updateAddButtonMargins` (batch 4); 2 hook includes | Low |
| `src/CMakeLists.txt` | compile the Limerino-owned page | 3-line append block at the tail of `SOURCE_FILES` (`# Limerino fork files…`); the same block hosts all future `limerino/` + `providers/limerino/` entries | Low — append-only at list tail |
| `src/singletons/Settings.hpp` | secondary extra-features auth needs a persisted store | one `QStringSetting limerinoAuthAccounts{"/limerino/auth/accounts", "[]"};` after the fork's existing `xChatterino7NoHttp2`; disjoint from `/accounts/uid<id>/` | Low |
| `default.nix` | nix package name reflects the fork | `pname = "technorino"` → `"limerino"` | Low |
| `flake.nix` | flake description reflects the fork | `description = "Technorino"` → `"Limerino"` | Low |
| `resources/icon.svg` | new Limerino app icon (master artwork) | content replaced, byte-same filename | Medium — binary; upstream icon change = binary conflict. Resolution: always ours |
| `resources/icon.png` | new Limerino app icon (Linux hicolor install, qrc) | content replaced; same 256x256 as before | Medium — see icon.svg |
| `resources/icon.ico` | Windows exe icon (via `cmake/resources/windows.rc.in`) | regenerated from new master; same 5 frames as upstream (16/32/48/64/256) | Medium — see icon.svg |
| `resources/chatterino.icns` | macOS bundle icon (`src/CMakeLists.txt:941`) | regenerated from new master; modern ic07–ic14 PNG chunks only (upstream's pre-OS-X il32/l8mk/is32/s8mk bitmap chunks dropped — irrelevant for Qt6 apps) | Medium — see icon.svg |

## Limerino-owned files

Entirely ours; will never conflict with upstream merges:

| Path | Created in |
|---|---|
| `FORK.md` | fork setup |
| `AGENTS.md` | fork setup |
| `src/limerino/` (incl. `src/limerino/LimerinoPage.{hpp,cpp}`, Limerino settings page) | fork setup / Limerino tab |
| `src/providers/limerino/` (`LimerinoAuth.{hpp,cpp}`) | extra-features auth subsystem |
| `src/providers/limerino/LimerinoApi.{hpp,cpp}` | Helix client for the ported plugin (tokens: primary-if-scopable else resolvers) |
| `src/providers/limerino/LimerinoErrors.{hpp,cpp}` | shared user-facing error mapping |
| `src/providers/limerino/LimerinoRateLimiter.{hpp,cpp}` | per-host request queue + 429 backoff |
| `src/providers/limerino/gql/` (`LimerinoGql`, `PersistedQueries`) | GQL client (persisted + inline) with central errors[] extraction; query/hashes transcribed from `pluginforreference/requests.lua` |
| `src/providers/limerino/commands/` (`LimerinoCommands.*`) | ported-command registration entry (batches 1-12) |
| `src/widgets/dialogs/LimerinoAuthDialog.{hpp,cpp}` | extra-features auth dialog (Device/Script/Accounts tabs) |
| `src/widgets/dialogs/limerino/LimerinoResultList.{hpp,cpp}` | reusable sortable/paginated result table (batches 1,2,5,9,10) |
| `src/widgets/dialogs/limerino/LimerinoResultDialog.{hpp,cpp}` | thin dialog shell for result lists |
| `src/widgets/dialogs/limerino/LimerinoPinView.{hpp,cpp}` | on-demand pinned-message banner (mounted via runtime layout insert, zero Split.cpp edits) |
| `src/widgets/dialogs/limerino/LimerinoPredictionDialog.{hpp,cpp}` | prediction manager window (create/history/lock/payout) |
| `resources/buttons/channelPoints-{dark,light}Mode.svg` | mod-toolbar predictions icon |
| `resources/limerino/limerinoauth.txt` | frozen reference auth script (byte-identical; Qt resource `:/limerino/limerinoauth.txt`) |

### Extra-features auth (secondary login)

A second, independent Twitch authorization (OAuth 2.0 Device Authorization Grant shape) used
only by extra features — completely separate from the primary login (`/accounts/uid<id>/`), with
zero diff on `src/widgets/dialogs/LoginDialog.*`.

- Store: `src/providers/limerino/LimerinoAuth.{hpp,cpp}` — JSON array in the
  `/limerino/auth/accounts` setting; validation via `id.twitch.tv/oauth2/validate` + Helix
  `users` + (best-effort) `moderation/channels`; locale-aware sorted; dedupe userId → token.
- Device login: `LimerinoAuth::DeviceLogin` (same module) — generation counter + `QPointer`
  guards, RFC error handling (`authorization_pending` / `slow_down` / `access_denied` /
  `expired_token`), 3 s poll floor, stops at `expires_in`.
- UI: `src/widgets/dialogs/LimerinoAuthDialog.{hpp,cpp}` (new dialog; Device Login / Script
  Login / Accounts), entered from the Extra features section of the Limerino settings page
  (live-updating summary via `accountsChanged`).
- Script fallback: frozen `resources/limerino/limerinoauth.txt` shown read-only; paste-token
  ingest runs through `normalizeToken()` and clears the clipboard afterward.
- Resolver API for feature code (not wired yet): `resolveModerationToken`,
  `resolveBroadcasterToken`, `resolveCurrentUserToken`, `resolveReadToken`,
  `authRequiredMessage`, `authExpiredMessage`. Local-only over cached account state.
- Invariants: never log token material (`redact()` on every stored error text), no telemetry
  piggybacking, no silent clipboard copies (user code copy is announced in the status label).
| `scripts/dev-build.sh` | fork setup |
| `scripts/dev-test.sh` | fork setup |
| `scripts/merge-upstream.sh` | fork setup |

## Cherry-picks

| Upstream SHA | Repo | Date | Reason | Status |
|---|---|---|---|---|
| _none yet_ | | | | |

## Deliberate non-changes

- Twitch client ID: UNCHANGED, intentionally. Do not touch. Same for 7TV/BTTV/FFZ API keys
  and endpoints (`src/providers/twitch/`, `src/providers/seventv/`, etc.).
- Executable/CMake target name: still `chatterino` (`project(chatterino …)` in `CMakeLists.txt`,
  `${EXECUTABLE_PROJECT}` in `src/CMakeLists.txt`), to keep `.CI/` and workflow scripts working.
  Branding is user-facing only.
- Settings/config path **shared with technorino/Chatterino7 on purpose (decided 2026-07-31)**:
  `%APPDATA%\Chatterino2` (win) / `~/.local/share/chatterino` (linux). Limerino *replaces*
  technorino on the machine rather than coexisting with it. `src/main.cpp` app name and
  `src/singletons/Paths.cpp` therefore stay untouched. If coexistence is ever wanted, this is
  the single riskiest change (needs `setApplicationName("limerino")` + Paths rewrite +
  opt-in first-run copy; see Phase 6 plan in git history).
- Windows AppUserModelID stays `SevenTV.Chatterino7TV` (`src/common/Version.cpp`) and the Inno
  `AppId` GUID stays upstream's (`.CI/chatterino-installer.iss`) — taskbar/toast identity and
  installer upgrade path intentionally shared with technorino/c7 (decided 2026-07-31).
- User-visible "Chatterino"/"Chatterino 7TV" branding (window titles, toasts, windows.rc,
  macOS bundle, .desktop/appdata, Inno app name/publisher) is kept; only "Technorino"
  became "Limerino". `TechnorinoPage` class/filenames, `SettingsTabId::Technorino`, and the
  `technorino.client_detection` filter identifier stay (internal/user-filter compatibility).
- Persisted settings keys (e.g. `/x-chatterino7/*`) and filter identifiers
  (e.g. `technorino.client_detection`) stay as-is — renaming orphans user data / breaks filters.
- No formatting or style changes to upstream code, ever.

## Delta check

    git diff --stat upstream-technorino..limerino
    git log limerino ^upstream-technorino --oneline

As of fork creation the delta is empty by definition (both branches at the same commit).
If the delta starts spreading across many upstream files, refactor toward hook points
(new files under `src/limerino/`, minimal call sites in upstream files).
