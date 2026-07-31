# AGENTS.md — Limerino

Instructions for AI agents working in this repository. You have no memory of past sessions:
everything you need is written down here and in `FORK.md`.

## Project

**Limerino** is a personal long-lived fork. Upstream chain (each fork tracks the one above):

1. `Chatterino/chatterino2` (`master`) — original
2. `SevenTV/chatterino7` (`chatterino7`) — adds 7TV support
3. `2547techno/technorino` (`technorino`) — adds Kick support etc.
4. `lagx/Limerino` (`limerino`) — **this repo**

Tech: **C++23 / Qt6 / CMake** (target C++ standard is set in `CMakeLists.txt` line ~298;
Ninja is the expected generator; ccache is used as compiler launcher).

**`FORK.md` is the source of truth for this fork's delta from upstream. Read it before
editing anything.** It defines the branch topology, merge policy, the ledger of modified
upstream files, and the commit conventions. Work happens on the `limerino` branch;
`upstream-technorino` is a pristine mirror — never commit to it.

## Golden rules

Violating any of these fails the task:

- **Never change the Twitch API client ID.** Same for 7TV/BTTV/FFZ API keys and endpoints.
  Not by search-and-replace, not by "helpfully" parameterizing. Leave them exactly as-is.
- **Never reformat, reorder includes, run clang-format/clang-tidy --fix, or refactor code
  you were not asked to change.** No "cleanup" commits. Only files you are editing.
- **Never rename the CMake target, the executable name (`chatterino`), or build output
  filenames.** `.CI/` and `.github/workflows/` hardcode them; renaming creates permanent
  merge conflicts. Branding is user-facing strings only.
- **Additive over invasive:** new code goes in new files. Upstream files get minimal hook
  points only (ideally: one include + one call site).
- **Every upstream file you touch must be logged in `FORK.md`** ("Modified upstream files"
  table) **in the same commit.**
- **Never** `git push --force`, `git reset --hard`, `git clean -fdx`, `git rebase`, or delete
  branches. If you think one is needed, STOP and ask.
- **Never resolve an upstream merge conflict unsupervised.** Explain both sides and let the
  human decide.

Also: do not invent file paths, CMake variables, or settings keys — read the actual files.
If you cannot find something, say so instead of guessing.

## Where new code goes

New Limerino features live under **`src/limerino/`**. Hook into upstream files with the
smallest possible edit: one `#include` plus one call site.

Gate risky/optional features behind a CMake option following the existing
`CHATTERINO_PLUGINS` / `CHATTERINO_SPELLCHECK` pattern:

- option declaration: `CMakeLists.txt` (~line 36–39)
- package lookup: `CMakeLists.txt` (~line 181–189)
- compile definition + link: `src/CMakeLists.txt` (~line 962–968 and 1041–1044)

## Build and test (Linux VPS)

Development happens on Windows, but builds run on a headless Linux VPS. Prerequisites:
Qt6 dev packages, boost, OpenSSL, hunspell, ninja-build, ccache (see `BUILDING_ON_LINUX.md`
for the package list), and initialized submodules:

    git submodule update --init --recursive

Build:

    ./scripts/dev-build.sh          # uses nproc jobs
    ./scripts/dev-build.sh -j 4     # override job count

The script configures with Ninja + RelWithDebInfo + ccache +
`-DCHATTERINO_SPELLCHECK=On -DBUILD_TESTS=On` **only if the build dir does not exist yet**,
then builds. To force a reconfigure, delete `build/`.

ccache is expected: a cold build is long, an incremental build is fast. **Linking is the RAM
spike** — on low-memory machines cap parallel jobs (e.g. `-j 2`/`-j 4`) or the linker may get
OOM-killed.

## Running tests

The suite is `chatterino-test` (googletest, registered with ctest via `gtest_discover_tests`,
binary in `build/bin`). The VPS has no display, so run headless:

    ./scripts/dev-test.sh

which wraps `xvfb-run -a ctest --test-dir build --output-on-failure`.

## Windows builds

**Never attempt to build for Windows locally or cross-compile.** Windows binaries come from
GitHub Actions (`.github/workflows/build.yml`) on push to `limerino`. Download the artifact
zip named `chatterino-windows-x86-64-Qt-<version>.zip` from the workflow run.

## Commit conventions

- Prefix every fork commit subject with `[limerino]`, e.g. `[limerino] feat: …`,
  `[limerino] ci: …`.
- One logical change per commit. Small commits are cherry-pickable and revertable; large
  ones are not.
- If a commit touches an upstream file, the `FORK.md` ledger update is part of the same
  commit.

## Workflow expectations for the agent

- **Before editing:** state a plan and the exact file list, and wait for approval.
- Prefer reading the specific files involved over broad codebase sweeps.
- **After editing:** run `./scripts/dev-build.sh` and iterate on compiler errors yourself
  rather than asking the human to.
- Dense, high-risk areas — **message rendering/layout, the emote pipeline, IRC and
  EventSub handling**: propose an approach before writing code, never freestyle.
- **If a change would touch more than ~3 upstream files, stop and discuss the design first.**
