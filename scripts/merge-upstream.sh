#!/usr/bin/env bash
# merge-upstream.sh — the FORK.md update cycle:
#   1. upstream-technorino: fast-forward to technorino/technorino
#   2. limerino: merge upstream-technorino
#   3. submodules: sync recursively
#
# Hard guarantees:
#   - refuses to run on a dirty working tree
#   - never auto-resolves conflicts; on conflict it lists the conflicted files
#     and exits nonzero, leaving the merge state for a human (`git merge --continue`
#     or `git merge --abort`)
#   - never pushes
set -euo pipefail

cd "$(dirname "$0")/.."
die() { echo "error: $*" >&2; exit 1; }

if [ -n "$(git status --porcelain)" ]; then
    die "working tree is dirty; commit or stash before merging upstream:"
fi
if git rev-parse -q --verify MERGE_HEAD >/dev/null 2>&1; then
    die "a merge is already in progress (MERGE_HEAD exists); finish or abort it first"
fi

echo "==> 1/3 ff-only update of upstream-technorino"
git switch upstream-technorino
git pull --ff-only technorino technorino

echo "==> 2/3 merge upstream-technorino into limerino"
git switch limerino
if ! git merge upstream-technorino; then
    echo >&2
    echo "MERGE CONFLICT — leaving it for a human, nothing auto-resolved." >&2
    echo "Conflicted files:" >&2
    git diff --name-only --diff-filter=U >&2
    echo >&2
    echo "Next: resolve by hand, then 'git merge --continue'; or 'git merge --abort' to bail." >&2
    exit 1
fi

echo "==> 3/3 submodules"
git submodule update --init --recursive

echo "==> done. Review, build (./scripts/dev-build.sh), and when happy push origin limerino."
