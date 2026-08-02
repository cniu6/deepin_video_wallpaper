#!/bin/bash
# 安装安全版动态壁纸插件到 Deepin 不可变系统 desktop-edge
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD="$ROOT/build"
SO="$BUILD/libdd-videowallpaper-plugin.so"
DEST="/usr/lib/x86_64-linux-gnu/dde-file-manager/plugins/desktop-edge"

mkdir -p "$BUILD"
cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD" -j"$(nproc)"

if [[ ! -f "$SO" ]]; then
  echo "build failed: $SO missing" >&2
  exit 1
fi

# Deepin 不可变系统：/usr 只读，必须走 immutable-ctl
pkexec deepin-immutable-ctl admin exec -- bash -c \
  "mkdir -p '$DEST' && cp -f '$SO' '$DEST/' && chmod 644 '$DEST/$(basename "$SO")' && ls -la '$DEST'"

systemctl --user restart 'dde-shell-plugin@org.deepin.ds.desktop.service'
echo "已安装并重启桌面。桌面空白处右键勾选 Video wallpaper。"
echo "视频目录: ~/Videos/video-wallpaper/ （可用 current.mp4 软链指定）"
