#!/bin/sh
set -eu
[ "$(id -u)" != 0 ] || { echo 'Run as your normal desktop user' >&2; exit 1; }
rm -f "$HOME/.local/bin/universal-mouse-app" "$HOME/.local/lib/universal-mouse-app/universal-mouse-app" "$HOME/.local/share/applications/universal-mouse-app.desktop" "$HOME/.local/share/icons/hicolor/scalable/apps/universal-mouse-app.svg"
rmdir "$HOME/.local/lib/universal-mouse-app" 2>/dev/null || true
printf 'App removed. Your saved presets and backups remain in ~/.config/universal-mouse-app.\n'
