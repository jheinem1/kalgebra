#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir="$repo_dir/build"
app_dir="$build_dir/flatpak-app"
if [[ ! -f "$repo_dir/external/analitza/CMakeLists.txt" ]]; then
    git -C "$repo_dir" submodule update --init external/analitza
fi
mkdir -p "$build_dir"
if [[ ! -f "$app_dir/metadata" ]]; then
    flatpak build-init "$app_dir" org.kde.kalgebra.Devel org.kde.Sdk org.kde.Platform 6.11
    base_dir=$(flatpak info --show-location io.qt.qtwebengine.BaseApp//6.11)
    # Copy without security xattrs for filesystems which do not support SELinux labels.
    cp -r "$base_dir/files/." "$app_dir/files/"
fi
flatpak build --filesystem="$repo_dir" "$app_dir" sh "$repo_dir/scripts/flatpak-build-inner.sh" "$repo_dir"
install -Dm755 "$repo_dir/scripts/kalgebra-fork" "$app_dir/files/bin/kalgebra-fork"
cp "$app_dir/files/share/applications/org.kde.kalgebra.desktop" "$app_dir/files/share/applications/org.kde.kalgebra.Devel.desktop"
sed -i 's/^Name=KAlgebra$/Name=KAlgebra (development fork)/' "$app_dir/files/share/applications/org.kde.kalgebra.Devel.desktop"
sed -i 's/^Exec=.*/Exec=kalgebra-fork %u/; s/^Icon=.*/Icon=org.kde.kalgebra.Devel/' "$app_dir/files/share/applications/org.kde.kalgebra.Devel.desktop"
for icon in "$app_dir/files/share/icons/hicolor/"*/apps/kalgebra.*; do
    [[ -f "$icon" ]] || continue
    cp "$icon" "${icon%/*}/org.kde.kalgebra.Devel.${icon##*.}"
done
sed -i '/^Name\[/d' "$app_dir/files/share/applications/org.kde.kalgebra.Devel.desktop"
rm "$app_dir/files/share/applications/org.kde.kalgebra.desktop"
cp "$app_dir/files/share/metainfo/org.kde.kalgebra.appdata.xml" "$app_dir/files/share/metainfo/org.kde.kalgebra.Devel.appdata.xml"
sed -i 's/org.kde.kalgebra/org.kde.kalgebra.Devel/g' "$app_dir/files/share/metainfo/org.kde.kalgebra.Devel.appdata.xml"
rm "$app_dir/files/share/metainfo/org.kde.kalgebra.appdata.xml"
# Recreate the generated exports when rebuilding an already finished tree.
rm -rf -- "$app_dir/export"
flatpak build-finish --command=kalgebra-fork --share=ipc --socket=wayland --socket=fallback-x11 \
    --device=dri --filesystem=xdg-config/kdeglobals:ro --talk-name=org.kde.kconfig.notify "$app_dir"
flatpak build-export "$build_dir/flatpak-repo" "$app_dir"
flatpak build-bundle "$build_dir/flatpak-repo" "$build_dir/kalgebra-fixes.flatpak" org.kde.kalgebra.Devel
printf 'Bundle: %s/kalgebra-fixes.flatpak\n' "$build_dir"
