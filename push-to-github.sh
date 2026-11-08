#!/usr/bin/env bash
# Creates a PRIVATE GitHub repo from this folder and pushes it, which triggers the macOS build.
# Needs: git and the GitHub CLI (gh), already signed in (run `gh auth login` once).
set -euo pipefail

REPO_NAME="${1:-rock-icons-drive}"

cd "$(dirname "$0")"

if ! command -v gh >/dev/null; then
  echo "Install the GitHub CLI first: https://cli.github.com  (macOS: brew install gh)"; exit 1
fi
gh auth status >/dev/null 2>&1 || { echo "Run: gh auth login"; exit 1; }

[ -d .git ] || git init -b main
git add -A
git commit -m "Rock Icons Drive: classic rock distortion voicings" || echo "(nothing new to commit)"

gh repo create "$REPO_NAME" --private --source=. --remote=origin --push

echo
echo "Pushed. Watch the build:  gh run watch"
echo "When it finishes, download the plugin:  gh run download -n RockIconsDrive-macOS"
