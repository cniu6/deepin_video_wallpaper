#!/bin/bash
set -euo pipefail
ROOT=/home/zerohh/Documents/codingfile/github/deepin_video_wallpaper
SO="$ROOT/build-opt/libdd-videowallpaper-plugin.so"
DEST=/usr/lib/x86_64-linux-gnu/dde-file-manager/plugins/desktop-edge
LOG=/tmp/vw-opt-install.log
exec >"$LOG" 2>&1
date
ls -la "$SO"
if [[ ! -f "$SO" ]]; then
  echo missing so
  exit 1
fi
# also stage to user path
mkdir -p "$HOME/.local/lib/dde-file-manager/plugins/desktop-edge"
cp -f "$SO" "$HOME/.local/lib/dde-file-manager/plugins/desktop-edge/"
# immutable system install
if command -v deepin-immutable-ctl >/dev/null 2>&1; then
  deepin-immutable-ctl admin exec -- bash -c "cp -f '$SO' '$DEST/libdd-videowallpaper-plugin.so' && chmod 644 '$DEST/libdd-videowallpaper-plugin.so' && ls -la '$DEST/libdd-videowallpaper-plugin.so'" || true
fi
# fallback sudo
if ! cmp -s "$SO" "$DEST/libdd-videowallpaper-plugin.so" 2>/dev/null; then
  sudo cp -f "$SO" "$DEST/libdd-videowallpaper-plugin.so" || true
fi
ls -la "$DEST/libdd-videowallpaper-plugin.so" || true
systemctl --user restart 'dde-shell-plugin@org.deepin.ds.desktop.service' || true
sleep 5
echo '--- CPU ---'
for i in 1 2 3 4 5 6 7 8; do
  echo -n "t$i Xorg="
  ps -C Xorg -o %cpu= | head -1 | tr -d ' \n'
  echo -n " dde="
  ps -eo %cpu,cmd | awk '/dde-shell -p org.deepin.ds.desktop/ && !/awk/ {print $1; exit}'
  sleep 1
done
echo done
date
