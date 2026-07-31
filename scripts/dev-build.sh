#!/usr/bin/env bash
# dev-build.sh — configure (first run only) + build Limerino on the Linux VPS.
# Configure: Ninja + RelWithDebInfo + ccache + -DCHATTERINO_SPELLCHECK=On -DBUILD_TESTS=On.
# Reconfigure explicitly by deleting build/.
#
# Note: linking is the RAM spike. On low-memory machines cap the job count
# (./scripts/dev-build.sh -j 2 or -j 4) or the linker may get OOM-killed.
set -euo pipefail

jobs="$(nproc 2>/dev/null || echo 2)"
while getopts "j:h" opt; do
    case "$opt" in
        j) jobs="$OPTARG" ;;
        *) echo "usage: $0 [-j JOBS]" >&2; exit 2 ;;
    esac
done
case "$jobs" in
    ''|*[!0-9]*) echo "error: -j expects a number, got '$jobs'" >&2; exit 2 ;;
esac

cd "$(dirname "$0")/.."
build_dir="build"

if [ ! -f lib/libcommuni/CMakeLists.txt ]; then
    echo "error: submodules not initialized. Run: git submodule update --init --recursive" >&2
    exit 1
fi

if [ ! -f "$build_dir/CMakeCache.txt" ]; then
    echo "==> configure (first run; later reconfigures: delete $build_dir/)"
    launcher_args=()
    if command -v ccache >/dev/null 2>&1; then
        launcher_args=(-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache)
    else
        echo "    warning: ccache not found; cold rebuilds will be slow" >&2
    fi
    cmake -B "$build_dir" -G Ninja \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        "${launcher_args[@]}" \
        -DCHATTERINO_SPELLCHECK=On \
        -DBUILD_TESTS=On
fi

echo "==> build (jobs=$jobs)"
echo "    hint: linking is the RAM spike — on low-RAM machines use: $0 -j 2"
cmake --build "$build_dir" -j "$jobs"
