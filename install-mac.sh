#!/usr/bin/env bash
# Installs Rock Icons Drive (AU + VST3) system-wide on macOS.
#
# Usage:  sudo ./install-mac.sh [path/to/RockIconsDrive-macOS.zip]
# Default zip location: ~/Downloads/RockIconsDrive-macOS.zip
set -euo pipefail

if [ "$(id -u)" -ne 0 ]; then
  echo "Run with sudo:  sudo $0 [zip]"; exit 1
fi

# Find the zip in the invoking user's Downloads folder when run via sudo.
USER_HOME="$(eval echo "~${SUDO_USER:-$USER}")"
ZIP="${1:-$USER_HOME/Downloads/RockIconsDrive-macOS.zip}"
[ -f "$ZIP" ] || { echo "Zip not found: $ZIP"; echo "Pass its path as the first argument."; exit 1; }

AU_DIR="/Library/Audio/Plug-Ins/Components"
VST3_DIR="/Library/Audio/Plug-Ins/VST3"

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

echo "Unzipping $ZIP ..."
ditto -x -k "$ZIP" "$TMP"

AU_SRC="$(find "$TMP" -maxdepth 3 -name 'Rock Icons Drive.component' -print -quit)"
VST3_SRC="$(find "$TMP" -maxdepth 3 -name 'Rock Icons Drive.vst3' -print -quit)"
[ -n "$AU_SRC" ] || { echo "No 'Rock Icons Drive.component' inside the zip."; exit 1; }

mkdir -p "$AU_DIR" "$VST3_DIR"

install_bundle() {
  local src="$1" dest_dir="$2" name
  name="$(basename "$src")"
  rm -rf "$dest_dir/$name"
  ditto "$src" "$dest_dir/$name"
  xattr -cr "$dest_dir/$name"                       # remove the download quarantine flag
  codesign --force --deep -s - "$dest_dir/$name"    # ad-hoc sign so it loads on Apple Silicon
  chown -R root:wheel "$dest_dir/$name"
  chmod -R go-w "$dest_dir/$name"
  echo "Installed $dest_dir/$name"
}

install_bundle "$AU_SRC" "$AU_DIR"
[ -n "$VST3_SRC" ] && install_bundle "$VST3_SRC" "$VST3_DIR"

# Make Logic re-scan Audio Units.
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo
echo "Validating the Audio Unit (auval) ..."
if sudo -u "${SUDO_USER:-$USER}" auval -v aufx Rdrv Pers 2>&1 | tail -n 15; then :; fi

echo
echo "Done. Quit and reopen Logic, then insert Rock Icons Drive on a guitar track (Audio FX > Personal)."
echo "If Logic still doesn't list it: Logic Pro > Settings > Plug-in Manager > Reset & Rescan Selection."
