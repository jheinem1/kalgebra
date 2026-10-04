#!/usr/bin/env sh
# SPDX-License-Identifier: GPL-2.0-or-later
set -eu
repo_dir="$1"
cmake -S "$repo_dir/external/analitza" -B "$repo_dir/build/flatpak-analitza" -G Ninja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=/app -DBUILD_TESTING=ON
cmake --build "$repo_dir/build/flatpak-analitza" --parallel "${BUILD_JOBS:-4}"
cmake --install "$repo_dir/build/flatpak-analitza"
QT_QPA_PLATFORM=offscreen ctest --test-dir "$repo_dir/build/flatpak-analitza" --output-on-failure
cmake -S "$repo_dir" -B "$repo_dir/build/flatpak-kalgebra" -G Ninja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=/app -DCMAKE_PREFIX_PATH=/app -DBUILD_TESTING=ON -DBUILD_DESKTOP=ON
cmake --build "$repo_dir/build/flatpak-kalgebra" --parallel "${BUILD_JOBS:-4}"
# This process is already confined by flatpak build. Disable only Chromium's
# nested sandbox for the headless regression tests.
QT_QPA_PLATFORM=offscreen QTWEBENGINE_DISABLE_SANDBOX=1 QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu \
    QTWEBENGINEPROCESS_PATH=/app/bin/QtWebEngineProcess QTWEBENGINE_RESOURCES_PATH=/app/resources \
    QTWEBENGINE_LOCALES_PATH=/app/translations/qtwebengine_locales \
    ctest --test-dir "$repo_dir/build/flatpak-kalgebra" --output-on-failure
cmake --install "$repo_dir/build/flatpak-kalgebra"
