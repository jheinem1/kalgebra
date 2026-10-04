#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir="$repo_dir/build"
prefix="${1:-$build_dir/install}"
mkdir -p "$prefix"
prefix=$(CDPATH= cd -- "$prefix" && pwd)
if [[ ! -f "$repo_dir/external/analitza/CMakeLists.txt" ]]; then
    git -C "$repo_dir" submodule update --init external/analitza
fi
cmake -S "$repo_dir/external/analitza" -B "$build_dir/analitza" -G Ninja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX="$prefix" -DBUILD_TESTING=ON
cmake --build "$build_dir/analitza" --parallel "${BUILD_JOBS:-4}"
cmake --install "$build_dir/analitza"
QT_QPA_PLATFORM=offscreen ctest --test-dir "$build_dir/analitza" --output-on-failure
cmake -S "$repo_dir" -B "$build_dir/kalgebra" -G Ninja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX="$prefix" \
    -DCMAKE_PREFIX_PATH="$prefix" -DBUILD_TESTING=ON -DBUILD_DESKTOP=ON
cmake --build "$build_dir/kalgebra" --parallel "${BUILD_JOBS:-4}"
QT_QPA_PLATFORM=offscreen QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu \
    ctest --test-dir "$build_dir/kalgebra" --output-on-failure
cmake --install "$build_dir/kalgebra"
printf 'Built KAlgebra at %s/bin/kalgebra\n' "$prefix"
