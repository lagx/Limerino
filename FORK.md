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
| `.gitignore` | ignore local `.ccache/` dir + reference captures | appended 2-line "Limerino local dev" section at the end; later (batch R-series) appended `pubsubreference/`, `newpubsubhermesreference/`, `pluginforreference/` — untracked-capture roots that may embed live credentials and must never be committed; (batch U1) appended `gqlreference/` — same reason | Low — append-only; re-add if upstream rewrites the tail |
| `.github/workflows/build.yml` | CI must build the `limerino` branch; the `nightly-build` prerelease (which force-moves a tag) must point at `limerino`, not upstream | two edits: `limerino` added to `on.push.branches`; `create-release` job `if:` now `refs/heads/limerino` | **High — this file is updated upstream all the time. Expect a conflict on most merges. Resolution is always: keep upstream's version of the file, then re-apply these two exact edits.** |
| `.github/workflows/create-installer.yml` | installer workflow must fire after builds on `limerino` | one edit: `limerino` added to the `workflow_run.branches` filter | **High — same rule as build.yml: keep upstream's file, re-apply this edit** |
| `src/common/Version.cpp` | fork identity in window title/About; commit links must point at this repo | two string literals: `fullVersion_` `"Technorino "` → `"Limerino "`; commit URL host → `github.com/lagx/Limerino`; F7: `appUserModelID_` → `Limerino.Limerino`, stale buildString example comment fixed | Low — narrow context, rarely touched upstream |
| `src/common/Version.hpp` | displayed version must not drift from the CMake version | one edit: hardcoded `CHATTERINO_VERSION "2.5.5"` → derived from `CHATTERINO_VERSION_STR` (CMake `PROJECT_VERSION` injected in src/CMakeLists.txt) | Low |
| `src/widgets/dialogs/SettingsDialog.cpp` | the fork needs its own settings tab | minimal hook: one `#include "limerino/LimerinoPage.hpp"` + one `addTab(...)` line (no id arg, icon `:/icon.png`); upstream Technorino tab label unchanged | Low |
| `src/controllers/commands/CommandController.hpp` | fork commands need a registration extension point | one tiny public passthrough `registerExternalCommand()` wrapping the private `registerCommand()` | Low |
| `src/controllers/commands/CommandController.cpp` | fork commands must be wired at startup | one include + one call `LimerinoCommands::initialize(*this)` + one include + one call `limerino::initializePubSub()` (Hermes bootstrap) at the tail of `initializeDefaults` + the passthrough impl | **Highest care — this file gets most upstream command additions; conflict resolution: keep upstream, re-add our lines at the end of the function** |
| `src/widgets/dialogs/UserInfoPopup.cpp` / `.hpp` | usercard gets a "Name history" option + a GQL-extras label | one include + one `LabelButton` + connect (pre-existing); batch U2: one include + one `LimerinoUserCardWidget` + one `setTarget()` call inside the existing Helix-success lambda (no `ui_` struct changes); batch F1: name-history dialog opened with `nullptr` parent (popup auto-close used to destroy the dialog mid-fetch → UAF crash); batch F4: connect `extrasChanged` → re-append sub suffix (IVR/GQL race fix) + `.hpp` `subageBaseText_` member | Low |
| `src/widgets/splits/SplitHeader.cpp` / `.hpp` | split menus + mod toolbar get fork actions | "Follow channel" + "View followers/following" menu lines (batch 2); predictions button member + creation + layout slot + vis hints in `updateIcons`/`updateAddButtonMargins` (batch 4); "Filter events..." conditional menu entry for `/events` + 2 hook includes (live-updates P0); "Nuke messages..." conditional menu entry gated on `hasModRights() && isTwitchOrKickChannel()` opening `LimerinoNukeDialog` + 1 include (batch N3) | Low |
| `src/widgets/dialogs/SelectChannelDialog.cpp` / `.hpp` | new-split dialog lists the fork's `/events` special channel like `/mentions` | one include + one "Events" radio block (label + connect) on the Twitch page + one `getSelectedChannel` branch + one `Misc` case in `setSelectedChannel` + 2 focus-chain special cases (last entry wrap, Channel-edit back-tab) + 2 `ui_` struct members | Low |
| `src/CMakeLists.txt` | compile the Limerino-owned page | 3-line append block at the tail of `SOURCE_FILES` (`# Limerino fork files…`); the same block hosts all future `limerino/` + `providers/limerino/` entries (live-updates P0 added `providers/limerino/pubsub/*`, `PubSubEventsChannel`, `LimerinoEventFilterDialog`; batch U1 added `providers/limerino/gql/LimerinoUserCardExtras`; batch U2 added `widgets/dialogs/limerino/LimerinoUserCardWidget`); F7: `CHATTERINO_VERSION_STR` define (PRIVATE on version lib, PUBLIC on chatterino-lib); F7: macOS bundle name/identifier rebranded | Low — append-only at list tail |
| `src/singletons/Settings.hpp` | secondary extra-features auth needs a persisted store | one `QStringSetting limerinoAuthAccounts{"/limerino/auth/accounts", "[]"};` after the fork's existing `xChatterino7NoHttp2`; disjoint from `/accounts/uid<id>/`; also `limerinoPubSubHiddenEventTypes` (`ChatterinoSetting<QStringList>`) for the `/events` filter and `limerinoAutoAcknowledgeChatWarnings` (batch P2) | Low |
| `default.nix` | nix package name reflects the fork | `pname = "technorino"` → `"limerino"` | Low |
| `flake.nix` | flake description reflects the fork | `description = "Technorino"` → `"Limerino"` | Low |
| `resources/icon.svg` | new Limerino app icon (master artwork) | content replaced, byte-same filename | Medium — binary; upstream icon change = binary conflict. Resolution: always ours |
| `resources/icon.png` | new Limerino app icon (Linux hicolor install, qrc) | content replaced; same 256x256 as before | Medium — see icon.svg |
| `resources/icon.ico` | Windows exe icon (via `cmake/resources/windows.rc.in`) | regenerated from new master; same 5 frames as upstream (16/32/48/64/256) | Medium — see icon.svg |
| `resources/chatterino.icns` | macOS bundle icon (`src/CMakeLists.txt:941`) | regenerated from new master; modern ic07–ic14 PNG chunks only (upstream's pre-OS-X il32/l8mk/is32/s8mk bitmap chunks dropped — irrelevant for Qt6 apps) | Medium — see icon.svg |
| `src/providers/twitch/PubSubClient.cpp` | inherited bug fix: UNLISTEN responses were recorded as LISTEN (`NonceInfo{.isListen = true}` in `encodeUnsubscription`), corrupting diag counters | one word: `.isListen = true` → `false` | Low |
| `src/providers/twitch/TwitchIrcServer.cpp` | `/events` must resolve to the Limerino events channel everywhere channel names resolve | one include + one `if` block in `getCustomChannel` | Low — same region as upstream's other special-channel routes |
| `src/providers/twitch/TwitchChannel.cpp` | per-channel Hermes topics must start/stop with each refresh cycle, and user topics re-resolve on every `userStateChanged` | one include + one call `limerino::ensureHermesChannelTopics(*this)` + one call `limerino::ensureHermesUserTopics()` in `refreshPubSub` | Low — append at that function's existing hook point |
| `src/messages/Link.hpp` | expose the chat-warning acknowledgement link inside chat messages | one enum value `ChatWarnAcknowledge` | Low |
| `src/widgets/helper/ChannelView.cpp` | dispatch the new link type | one include + one switch case `Link::ChatWarnAcknowledge` | Low |
| `tests/CMakeLists.txt` | build the Limerino PubSub controller tests + highlight-group value tests + resolver tests | three entries: `tests/src/LimerinoPubSub.cpp`, `tests/src/LimerinoHighlightGroup.cpp`, `tests/src/LimerinoHighlightGroups.cpp`; batch U5 added `tests/src/LimerinoUserCardExtras.cpp`; batch N1 added `tests/src/LimerinoMatcher.cpp`; batch N2 added `tests/src/LimerinoNukePlan.cpp` | Low — append-only |
| `src/controllers/highlights/HighlightPhrase.hpp` | per-channel highlights: each phrase/user/badge belongs to one group | added `QUuid groupId_` member + defaulted trailing ctor param (`QUuid()` = Default/legacy-global); serde writes `groupId` only when non-null and reads it tolerantly | Low |
| `src/controllers/highlights/HighlightPhrase.cpp` | same | delegating ctor + `groupId_` init + `std::tie` extension in `operator==` + `groupId()` getter | Low |
| `src/controllers/highlights/HighlightBadge.hpp` | same | same shape as HighlightPhrase | Low |
| `src/controllers/highlights/HighlightBadge.cpp` | same | same shape as HighlightPhrase | Low |
| `src/singletons/Settings.hpp` | register `/highlighting/groups` store | one include + `ChatterinoSetting<std::vector<HighlightGroup>>` + `SignalVector<HighlightGroup> highlightGroups`; batch N4 added `limerinoNukePresets` (`QStringSetting`, presets as JSON); batch N5 added `limerinoAutoActions` (`QStringSetting`, rules as JSON) | Low |
| `src/singletons/Settings.cpp` | initialise the vector + create Default group on startup | two includes + one `initializeSignalVector` call + `new HighlightGroupController(*this, this)` after `instance_ = this`; batch N5: constructs `LimerinoAutoActionController` next to it | Low |
| `src/CMakeLists.txt` | compile the group data model + UI | fourteen entries appended to the tail `# Limerino fork files` block (`providers/limerino/highlights/HighlightGroup*`, `HighlightGroupChannelKey*`, `HighlightGroupChannels*`, `HighlightGroupCellDelegate*`, `HighlightGroupDialog*`, `HighlightGroupMenu*`) | Low — append-only |
| `src/controllers/highlights/HighlightController.hpp` | resolver needs a channel-keyed overload + caches | added `GroupedHighlightCheck` (check + groupId; null = global), `check(... channelKey)` overload, private `resolveChecks`/`runChecks`, two `QHash` caches | Medium |
| `src/controllers/highlights/HighlightController.cpp` | rebuild splits global vs groupable; per-channel resolve | six builders emit GroupedHighlightCheck; message/user/badge builders record phrase.groupId()/badge.groupId(); `rebuildChecks` clears both caches; legacy `check()` delegates with a NUL-containing sentinel key; per-channel `resolveChecks` caches by group-set key | Medium |
| `src/messages/MessageBuilder.cpp` | Twitch path must thread the channel key | one include + 5 lines in `parseHighlights` building the key from `message().platform + channelName` | High — hot file, vigilant merge |
| `src/providers/kick/KickMessageBuilder.cpp` | Kick path must thread the channel key | one include + 3 lines in `processHighlights` using `builder.channel()` | Low |
| `src/providers/twitch/eventsub/Connection.cpp` | automod path must thread the broadcaster channel key | one include + 2 lines in the automod GUI-thread lambda | Low |
| `src/controllers/highlights/HighlightModel.cpp` | keep groupId when roundtripping a row into a HighlightPhrase | one-line pass-through (replaced by H3's cell-derived variant) | Low |
| `src/controllers/highlights/UserHighlightModel.cpp` | same for user highlights | one line | Low |
| `src/controllers/highlights/BadgeHighlightModel.cpp` | same for badge highlights | one line | Low |
| `src/controllers/highlights/HighlightModel.hpp` | Group column on the messages table | `Group = 8` before COUNT; F3: `refreshGroupCells()` + `groupRefreshHolder_` member | Low |
| `src/controllers/highlights/HighlightModel.cpp` | populate Group cell / read groupId back from it / blank it on the 11 pinned rows | three blocks in getRowFromItem/getItemFromRow/afterInit; F3: ctor connects `highlightGroups.delayedItemsChanged` → `refreshGroupCells()` (rebuilds every non-custom row's Group option roles + re-derives its display name) | Low |
| `src/controllers/highlights/UserHighlightModel.cpp` | identical handling for the users table | mirrors HighlightModel (shared Column enum) + SelfMessageRow blanking; F3: same ctor hook + `refreshGroupCells()`; hpp gains the holder + method decl | Low |
| `src/controllers/highlights/BadgeHighlightModel.hpp` | Group column on the badges table | `Group = 6` before COUNT; F3: `refreshGroupCells()` + `groupRefreshHolder_` member; cpp gains the same ctor hook + method | Low |
| `src/widgets/settingspages/HighlightingPage.cpp` | wire Group column, delegate, and manager button on all three tabs | 2 includes, 3 title entries, 3 delegate hooks, 3 "Manage groups..." buttons; F3: one `setColumnWidth(Group, 140)` line in each of the 3 resize blocks | Low-moderate |
| `src/widgets/splits/SplitHeader.cpp` | split context menu shows which highlight groups apply to the channel | one include + one 5-line conditional call to `limerino::buildHighlightGroupsMenuEntry` after "Set filters" | Low |
| `CMakeLists.txt` | adopt c7 version numbering (`7.x.x`) so Limerino versions align with the c7/technorino lineage, not c2 | one-line `VERSION 2.5.5` → `7.5.5` in `project()` (F7: now the single source of truth for all version surfaces); F7: `DESCRIPTION` + `HOMEPAGE_URL` rebranded | Low — single line; c7 merges never touch this line (they inherit c2's value, then bump on their own *release tags*, not in-tree) |
| `README.md` | replace technorino's header with a Limerino header | lines 1–13 swapped: Limerino logo (`resources/icon.svg`), H1, one-line description with Chatterino/Technorino hyperlinks, `---` separator | Low — top-of-file block, upstream rewrites are rare and surgical |

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
| `src/providers/limerino/gql/LimerinoUserCardExtras.{hpp,cpp}` | combined usercard GQL fetch (language tag / team / sub detail) + 5-min per-user cache; doc authored from `gqlreference/gql/` shapes (batch U1); batch F4: `thirdPartySKU` added to selection set + struct + parse |
| `src/widgets/dialogs/limerino/LimerinoUserCardWidget.{hpp,cpp}` | per-usercard extras label (language tag top-right in the header box; team + sub rows later); QPointer + request-generation teardown (batch U2); batch F4: unknown platform strings render raw instead of hidden, SKU chip, `extrasChanged` signal |
| `src/providers/limerino/commands/` (`LimerinoCommands.*`) | ported-command registration entry (batches 1-12) |
| `src/widgets/dialogs/LimerinoAuthDialog.{hpp,cpp}` | extra-features auth dialog (Device/Accounts tabs; Script tab removed in F2 — clickable verification link, button state machine, success line) |
| `src/widgets/dialogs/limerino/LimerinoResultList.{hpp,cpp}` | reusable sortable/paginated result table (batches 1,2,5,9,10) |
| `src/widgets/dialogs/limerino/LimerinoResultDialog.{hpp,cpp}` | thin dialog shell for result lists |
| `src/widgets/dialogs/limerino/LimerinoPinView.{hpp,cpp}` | on-demand pinned-message banner (mounted via runtime layout insert, zero Split.cpp edits) |
| `src/widgets/dialogs/limerino/LimerinoPredictionDialog.{hpp,cpp}` | prediction manager window (create/history/lock/payout); extended by P4 with a Hermes live-data pane and the auto-acknowledge-warnings setting |
| `resources/buttons/channelPoints-{dark,light}Mode.svg` | mod-toolbar predictions icon |
| `src/providers/limerino/pubsub/` (`HermesMessages/Client/Manager`, `LimerinoPubSubController`, `LimerinoPubSubTopics`) | Hermes live-updates transport + topic-intent controller (batch P0); batch F6: `PubSubEvent` gains `category` + raw `payload`, generic fallback never prints raw topics |
| `src/providers/limerino/pubsub/HermesChannelTopics.{hpp,cpp}` | raid/polls/predictions channel topics (batch P1) |
| `src/providers/limerino/pubsub/HermesUserTopics.{hpp,cpp}` | authenticated user topics incl. chatrooms-user-v1 warn/ack flow (batch P2) |
| `src/limerino/PubSubEventsChannel.{hpp,cpp}` | `/events` special channel + per-event-type filter storage (batch P0; renamed from `/pubsub-events` in F5); batch F6: events render as structured messages (themed category chip, plain text, raw topic+payload in chip tooltip, optional #channel jump link) |
| `src/widgets/dialogs/limerino/LimerinoEventFilterDialog.{hpp,cpp}` | events-channel filter checklist (batch P0) |
| **Hermes live-updates topics (as of batch R2)** | inventory: channel `raid` / `polls` / `predictions-channel-v1` (unauth); user `chatrooms-user-v1` / `community-points-user-v1` / `predictions-user-v1` / `follows` (LimerinoAuth). Classic `community-points-channel-v1` + `pinned-chat-updates-v1` remain on upstream `wss://pubsub-edge.twitch.tv` — intentionally NOT migrated to Hermes (the R-series reference does not confirm them over Hermes, so the move precondition is unmet). **Provenance:** prediction parse shapes re-derived from `newpubsubhermesreference/hermes/` (authoritative for the R batches; earlier `pubsubreference/` kept as history); persisted-query hashes from `pluginforreference/`. All three reference folders are untracked working-tree captures, gitignored (may embed credentials — never commit). |
| `tests/src/LimerinoPubSub.cpp` | controller state-machine tests over a recording sink double (batch P0) |
| `src/providers/limerino/matcher/` (`LimerinoMatcher`, `LimerinoMatcherValidation`) | shared content/sender matcher + one pair validator (`validateMatcherPair`) enforcing the both-empty rejection; regex + literal modes mirroring `HighlightPhrase`; precompiled `QRegularExpression`, never per message (batch N1) |
| `tests/src/LimerinoMatcher.cpp` | matcher unit tests: single semantics, invalid regex never falls back, both-empty pair rejection, serde round-trip (batch N1) |
| `src/providers/limerino/nuke/` (`NukePlan`, `NukeEngine`) | nuke plan types + `buildPlan(snapshot, platform, channel, self, matchers, lookback, action)`: pure, no network/settings/app state. Dedups by sender for ban/timeout/warn, per-message IDs for delete; self + broadcaster excluded; already-deleted/system/timeout-record messages filtered; Kick warns produce zero targets (batch N2) |
| `src/providers/limerino/nuke/NukePreset.{hpp,cpp}` + `NukePresetsStore.{hpp,cpp}` | named nuke configurations (matchers + lookback + action + params) persisted under `/limerino/nukePresets` (batch N4) |
| `src/providers/limerino/autoactions/` | auto-action rule model (`LimerinoAutoAction`, scope semantics mirror HighlightGroup), store, and per-channel resolver (`LimerinoAutoActionController`) with a per-channel cached shared_ptr matching the highlight resolver's shape; placeholders in `AutoActionPlaceholders.{hpp,cpp}`, `{msg.text}` deliberately excluded (batch N5) |
| `tests/src/LimerinoAutoActions.cpp` | auto-action value-type tests: scope, serde round-trip, normalisation (batch N5) |
| `tests/src/LimerinoNukePlan.cpp` | nuke plan tests: request validation, empty buffer, lookback-overflow flag, exclusion flags, dedup by user, delete per-message, compound action, invalid regex, Kick warn (batch N2) |
| `src/providers/limerino/nuke/NukeExecutor.{hpp,cpp}` | serial execution of a NukePlan through `LimerinoApi` moderation wrappers (throttled via LimerinoRateLimiter), Kick via `getKickApi()`; progress signals, cancel(), global `/cancelnuke` singleton (batch N3) |
| `src/widgets/dialogs/limerino/LimerinoNukeDialog.{hpp,cpp}` | nuke dialog: matchers with live regex validation, Preview->Execute gating, buffer-coverage banner, irreversible-delete note, progress/cancel/results (batch N3) |
| `src/providers/limerino/highlights/HighlightGroup.{hpp,cpp}` | highlight-group value type: `AllExcept`/`Only` scope, `DEFAULT_ID`, channel-list normalisation, pajlada serde (`/highlighting/groups` schema) (batch H1) |
| `src/providers/limerino/highlights/HighlightGroupChannelKey.{hpp,cpp}` | `"platform:name"` / `special:*` key derivation from `Channel` and from `MessagePlatform + name` (batch H1) |
| `src/providers/limerino/highlights/HighlightGroupController.{hpp,cpp}` | guarantees the Default group exists at startup; `findGroup`/`matchingGroupIds` helpers (batch H1) |
| `tests/src/LimerinoHighlightGroup.cpp` | value-type tests: normalisation, `matches()` (both scopes + sentinels), serde round-trip, absent-ID → Default migration (batch H1) |
| `tests/src/LimerinoHighlightGroups.cpp` | resolver tests: per-channel filtering, Only/AllExcept scoping, legacy overload equivalence, sentinel keys, cache invalidation via rebuild triggers (batch H2) |
| `tests/src/LimerinoUserCardExtras.cpp` | parse-response tests: full response, empty, partial-with-field-nulls, null team, null subscription (batch U5) |
| `src/providers/limerino/highlights/HighlightGroupCellDelegate.{hpp,cpp}` | per-row Group combobox delegate with a trailing "New group…" sentinel that opens the manager dialog (batch H3) |
| `src/providers/limerino/highlights/HighlightGroupDialog.{hpp,cpp}` | group manager dialog: list + add/delete with confirmation and member recount, scope radio, channel-list editor with autocomplete + typo warning, createGroupModal for in-table group creation (batch H3) |
| `src/providers/limerino/highlights/HighlightGroupChannels.{hpp,cpp}` | known-channel enumeration for the editor, sourced from open splits + persisted group channels (batch H3) |
| `src/providers/limerino/highlights/HighlightGroupMenu.{hpp,cpp}` | read-only "Highlight groups" submenu in the split context menu; entry per matching group + Manage groups... action; no state is written (batch H4) |

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
- UI: `src/widgets/dialogs/LimerinoAuthDialog.{hpp,cpp}` (new dialog; Device Login /
  Accounts), entered from the Extra features section of the Limerino settings page
  (live-updating summary via `accountsChanged`). Batch F2 removed the Script Login tab and
  the frozen `resources/limerino/limerinoauth.txt` artifact; the button now reflects flow
  state and the verification URI is a clickable incognito link.
- Resolver API for feature code (not wired yet): `resolveModerationToken`,
  `resolveBroadcasterToken`, `resolveCurrentUserToken`, `resolveReadToken`,
  `authRequiredMessage`, `authExpiredMessage`. Local-only over cached account state.
- Invariants: never log token material (`redact()` on every stored error text), no telemetry
  piggybacking, no silent clipboard copies (user code copy is announced in the status label).

### UserCard GQL enrichments (language tag, Twitch team, subscription detail)

One combined inline GQL doc `LimerinoUserCardExtras($id, $channelID)` (rate-limited, resolved
via `LimerinoAuth::resolveReadToken`, cached per user ID for 5 min) fetches three `User`-node
field sets for the usercard's target. Sources: `gqlreference/gql/{fragments.js,user/queries.js}`;
**`gqlreference/` is a gitignored untracked-capture root** (entry lives under `.gitignore`'s
"Fork reference captures"). It holds no client IDs and no captured response bodies — only the
module source. Display lands in
`src/widgets/dialogs/limerino/LimerinoUserCardWidget.{hpp,cpp}` (one `UserInfoPopup.cpp` hook:
1 include + 1 widget placement + 1 `extrasChanged` connect + 1 `setTarget()` hand-off +
sub-age base/suffix handling; batch F4 made the suffix re-apply when late GQL extras land,
which is the race that used to hide platform/SKU entirely).

| Field | Data path | Auth (G0.2 analysis) | Renders |
|---|---|---|---|
| Preferred language tag | `user.settings.preferredLanguageTag` | any user token (resolver: `resolveReadToken`); perm-miss → `null`, no `errors[]` | bare tag top-right of upper half; hidden (zero footprint) when absent |
| Primary team | `user.primaryTeam.name` (singular, not a list) | any user token; team-less → `null` | one dimmed label under the tag when present; nothing when absent |
| Subscription detail | `user.relationship(targetUserID).subscriptionBenefit{platform,purchasedWithPrime,tier,gift.isGift}`+`subscriptionTenure.months` | any user token; not mod-gated; `null` → not subbed | appended onto existing `★ Tier…` IVR sub-age row via ` · web/Android/iOS/Prime, gift, Tier 2/3` (unrecognized platforms hidden; field-by-field degradation, never blanks the base row) |

Open questions (rulings issued before build): show single team only (primaryTeam is not a
list); platform rendered raw but unrecognized wire values hidden; tag rendered verbatim with no
tooltip; tag placed immediately right of user ID; 5-min cache; combined-inline doc is acceptable
(`gqlreference/` has no literal all-three capture); `/gqlreference/` added to `.gitignore`.

### Nuke (moderation retro-action) and Auto Actions

Two features built on one shared matcher (`src/providers/limerino/matcher/`).

**Shared matcher semantics** (`LimerinoMatcher`):

- Empty pattern is an absent constraint and matches everything.
- Message-content matcher **AND** sender matcher both must match.
- Both matchers empty is a validation error — refused by the editor *and* the
  nuke-engine planning call (would otherwise select the whole buffer).
- Regex tolerated on compile failure: `matches()` returns `false`, editor shows
  `compileError()` text, no fallback to substring matching.
- Case-insensitive by default; per-matcher "Case sensitive" toggle, mirroring
  `HighlightPhrase`'s `isRegex`/`isCaseSensitive` idiom.

**Nuke (`/cancelnuke`, `/unnuke`, split menu ➜ "Nuke messages...")**

- Moderator-only entry point in the split header dropdown, gated on
  `hasModRights() && isTwitchOrKickChannel()`.
- Message buffer coverage: `Channel::getMessageSnapshot()` counts `N` messages
  per the `/misc/scrollback/splitLimit` setting (default 1000). A 600 s lookback
  may therefore only cover the last few seconds in a busy channel. The Preview
  shows the buffer's real coverage `HH:MM:SS → HH:MM:SS` and a prominent
  warning banner when `lookbackExceedsBuffer` is true.
- `BuildPlan` (pure) deduplicates by sender for Ban/Timeout/Warn; Delete is
  per message. Self and the channel broadcaster are always excluded; their
  matchers never appear in the target list.
- Already-deleted (`MessageFlag::Disabled`) and timeout/deletion record
  messages (`Timeout`, `ClearChat`, `ModerationAction`, `System`, `Whisper`)
  are excluded from matching.
- Actions route through `LimerinoApi::{ban,warn,deleteChatMessage,unban}User`
  which submit canonical Helix requests through `LimerinoRateLimiter`. Kick
  supports ban/timeout/delete but **not warn** (no Kick endpoint) — the Warn
  option is hidden from the dialog on Kick channels.
- `/cancelnuke` aborts the running nuke (one in flight globally). `/unnuke`
  reverses the last completed nuke's bans/timeouts in that channel.
  **Deletes cannot be undone**; this is stated in the dialog before execute.
- Presets (`limerinoNukePresets`): matchers + lookback + action + params.

**Auto Actions (`limerinoAutoActions`, settings → Limerino tab ➜ Auto actions)**

- One rule = matchers + `Scope` (AllExcept-with-empty-list = everywhere, or
  Only-with-list using the shared `highlightChannelKey` scheme) + command
  template + enabled flag + per-rule cooldown.
- Resolver caches the applicable rule list per channel as a
  `std::shared_ptr<const std::vector<LimerinoAutoAction>>` — same shape as the
  highlight-group per-channel cache. Cache invalidates on `delayedItemsChanged`
  of the setting.
- Command dispatch uses `getApp()->getCommands()->execCommand(...)`, so every
  built-in, Limerino, and user-defined command works ("/ban", "/timeout",
  "/warn", "/delete", ...).
- Runtime safety (all unconditional):
  - Never fires on the current user's own messages.
  - Per-rule cooldown in seconds; the N7 rule editor defaults to 10.
  - No re-entry: while an evaluation is in progress the global guard refuses
    re-entrant calls (this also breaks an action producing a message that
    triggers the same rule).
  - Moderator check per channel before any dispatch (`hasModRights()`).
  - A message emitted by a fired action cannot itself be evaluated: the guard
    above catches that the action's echo arrives inside the same call.
- Placeholders expanded at fire time (`AutoActionPlaceholders.cpp`):
  - `{msg.id}` `{sender.name}` `{sender.displayName}` `{sender.id}`
    `{channel.name}` `{channel.id}` `{platform}`.
  - `{{` and `}}` produce literal braces.
  - Unknown placeholders expand to empty and log a warning once per rule.
  - An unavailable required value (`{msg.id}` on a channel that lacks message
    IDs, `{channel.id}` on a special channel) **skips the action** and logs —
    a command is never sent with an empty ID argument.
  - `{msg.text}` is **excluded** in v1: the message text is attacker-controlled
    and would inject directly into a command string. Any future reintroduction
    must be sanitised first (strip leading `/` and `.`, collapse newlines,
    cap length).

| `scripts/dev-build.sh` | fork setup |
| `scripts/dev-test.sh` | fork setup |
| `scripts/merge-upstream.sh` | fork setup |

## Cherry-picks

| Upstream SHA | Repo | Date | Reason | Status |
|---|---|---|---|---|
| _none yet_ | | | | |

## Deliberate non-changes

- 7TV/BTTV/FFZ API keys and endpoints (`src/providers/twitch/`, `src/providers/seventv/`,
  etc.): UNCHANGED, intentionally. Do not touch.
- Twitch **extra-features auth** client ID: `kd1unb4b3q4t58fwlpcbzcbnm76a8fp` — the Twitch
  web / front-end first-party client. **Changed once (batch R1, 2026-08)** from the original
  Android-TV id (`ue6666qo983tsx6so1t0vnawi233wa`) still shown in `pluginforreference/`.
  Lives in `LimerinoAuth.cpp` (`CLIENT_ID`); every GQL/Helix/device-flow/Hermes-WS-URL call
  reads it from that one constant. The new id supports the OAuth 2.0 Device Authorization
  Grant (twitch.tv/activate) — verified 2026-08 — and the public refresh_token grant. Do not
  change it again without re-verifying the device grant for the replacement; OAuth client
  registrations, not the protocol, gate that flow.
- Twitch **primary login** (chatterino's own public client id): UNCHANGED — see the row above.
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
