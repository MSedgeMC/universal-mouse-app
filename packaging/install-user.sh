#!/bin/sh
# Install the prebuilt tar package for the current user. No compilation.
set -eu
[ "$(id -u)" != 0 ] || { echo 'Run as your normal desktop user' >&2; exit 1; }
package_dir=$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)
[ -x "$package_dir/bin/universal-mouse-app" ] || { echo 'Run this script from the extracted release archive' >&2; exit 1; }
app_dir="$HOME/.local/lib/universal-mouse-app"
[ ! -L "$app_dir" ] || { echo 'Installation directory must not be a symlink' >&2; exit 1; }
mkdir -p "$app_dir" "$HOME/.local/bin" "$HOME/.local/share/applications" "$HOME/.local/share/icons/hicolor/scalable/apps"
install -m 755 "$package_dir/bin/universal-mouse-app" "$app_dir/universal-mouse-app"
ln -sfn "$app_dir/universal-mouse-app" "$HOME/.local/bin/universal-mouse-app"
ln -sfn "$app_dir/universal-mouse-app" "$HOME/.local/bin/orbit-mouse"
install -m 644 "$package_dir/share/icons/hicolor/scalable/apps/universal-mouse-app.svg" "$HOME/.local/share/icons/hicolor/scalable/apps/universal-mouse-app.svg"
# Quote the full path according to the desktop-entry Exec grammar.
app_exec=$(printf '%s' "$app_dir/universal-mouse-app" | sed 's/[\\"`$]/\\&/g')
sed "s|^Exec=.*|Exec=\"$app_exec\"|" "$package_dir/share/applications/universal-mouse-app.desktop" > "$HOME/.local/share/applications/universal-mouse-app.desktop"
if command -v update-desktop-database >/dev/null 2>&1; then update-desktop-database "$HOME/.local/share/applications"; fi
printf 'Installed Orbit. Start it from the application menu or run orbit-mouse.\n'
printf 'If mouse access is unavailable, install the packaged scoped udev rule and reconnect it.\n'
