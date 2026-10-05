#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# minColorAE. Copyright (C) 2026 cbkow.
#
# package.sh <version> <mac .plugin dir> <win .aex dir> <out dir>
# Builds minColorAE-<version>-macOS.zip and minColorAE-<version>-Windows.zip: the
# effects, the panel (from build-release), the presets, an INSTALL.md, LICENSE,
# NOTICE. The mac plugins must already be signed (and notarized/stapled).
set -eu
V="$1"; MAC="$2"; WIN="$3"; OUT="$4"
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
PANEL="$REPO/build-release/panel/minColor.jsx"
mkdir -p "$OUT"

stage() {   # stage <platform> <dir> <install-table rows>
  P="$1"; D="$2"
  rm -rf "$D"; mkdir -p "$D/Plug-ins/minColor" "$D/ScriptUI Panels" "$D/User Presets/minColor"
  cp "$PANEL" "$D/ScriptUI Panels/minColor.jsx"
  cp "$REPO"/presets/ae/minColor/*.ffx "$D/User Presets/minColor/"
  cp "$REPO/LICENSE" "$REPO/NOTICE" "$REPO/CHANGELOG.md" "$D/"
  ROWS="$3" VER="$V" PLAT="$P" python3 -c 'import os, sys; t = open(sys.argv[1]).read(); sys.stdout.write(t.replace("@VERSION@", os.environ["VER"]).replace("@PLATFORM@", os.environ["PLAT"]).replace("@ROWS@", os.environ["ROWS"]))' "$REPO/tools/release/INSTALL.md" > "$D/INSTALL.md"
}

M="$OUT/minColorAE-$V-macOS"
stage macOS "$M" "| \`Plug-ins/minColor\` (the folder) | \`/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/\` |
| \`ScriptUI Panels/minColor.jsx\` | \`~/Library/Preferences/Adobe/After Effects/26.5/Scripts/ScriptUI Panels/\` |
| \`User Presets/minColor\` (the folder) | \`~/Documents/Adobe/After Effects 2026/User Presets/\` |"
ditto "$MAC" "$M/Plug-ins/minColor"
(cd "$OUT" && rm -f "minColorAE-$V-macOS.zip" && ditto -c -k --keepParent "minColorAE-$V-macOS" "minColorAE-$V-macOS.zip")

W="$OUT/minColorAE-$V-Windows"
stage Windows "$W" "| \`Plug-ins\\minColor\` (the folder) | \`C:\\Program Files\\Adobe\\Common\\Plug-ins\\7.0\\MediaCore\\\` (needs administrator) |
| \`ScriptUI Panels\\minColor.jsx\` | \`%APPDATA%\\Adobe\\After Effects\\26.5\\Scripts\\ScriptUI Panels\\\` |
| \`User Presets\\minColor\` (the folder) | \`Documents\\Adobe\\After Effects 2026\\User Presets\\\` |"
cp "$WIN"/*.aex "$W/Plug-ins/minColor/"
(cd "$OUT" && rm -f "minColorAE-$V-Windows.zip" && zip -qr "minColorAE-$V-Windows.zip" "minColorAE-$V-Windows")

cd "$OUT" && shasum -a 256 "minColorAE-$V-macOS.zip" "minColorAE-$V-Windows.zip"
