#!/usr/bin/env bash
# 探测本机能否走 DMA-BUF/硬解导出（不改系统）
set -euo pipefail
echo "=== session ==="
echo "XDG_SESSION_TYPE=${XDG_SESSION_TYPE:-unset}"
echo "WAYLAND_DISPLAY=${WAYLAND_DISPLAY:-none}"
echo "DISPLAY=${DISPLAY:-none}"
echo "=== hwaccels ==="
ffmpeg -hide_banner -hwaccels 2>&1 | head -20
echo "=== cuda filters ==="
ffmpeg -hide_banner -filters 2>&1 | grep -i cuda | head -20 || echo "(no cuda filters in ffmpeg build)"
echo "=== DRM ==="
ls -l /dev/dri 2>/dev/null || echo "no /dev/dri"
pkg-config --exists libdrm && pkg-config --modversion libdrm || echo "no libdrm pc"
echo "=== conclusion ==="
if [[ "${XDG_SESSION_TYPE:-}" == "wayland" ]]; then
  echo "Wayland session: buffer import path may exist; still needs dde wallpaper API."
else
  echo "X11 session: dde desktop-edge wallpaper has no public DMA-BUF plane API."
  echo "Plugin remains CPU QImage -> QWidget paint -> X damage."
fi
