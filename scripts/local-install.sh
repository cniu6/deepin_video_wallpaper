#!/usr/bin/env bash
# 本机一键：编译 → 打 deb → 装到不可变 /usr → 重启桌面（不推送、不联网）
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

echo "==> [1/4] 打 deb"
bash "$ROOT/scripts/build-deb.sh"

DEB="$(ls -1t "$ROOT"/dist/deepin-video-wallpaper_*.deb | head -1)"
if [[ ! -f "$DEB" ]]; then
  echo "找不到 deb" >&2
  exit 1
fi
echo " deb: $DEB"

echo "==> [2/4] 安装 so 到系统（不可变层；dpkg 配置步不在 root 里同步 restart 桌面）"
SO_SRC="$ROOT/build-deb/cmake/libdd-videowallpaper-plugin.so"
[[ -f "$SO_SRC" ]] || SO_SRC="$ROOT/build-opt/libdd-videowallpaper-plugin.so"
DEST=/usr/lib/x86_64-linux-gnu/dde-file-manager/plugins/desktop-edge
install_cmd="mkdir -p '$DEST' && cp -f '$SO_SRC' '$DEST/libdd-videowallpaper-plugin.so' && chmod 644 '$DEST/libdd-videowallpaper-plugin.so' && ls -la '$DEST/libdd-videowallpaper-plugin.so'"

# 先把可能卡住的 postinst 修掉再 dpkg --configure；优先直接拷 so（最快）
if command -v deepin-immutable-ctl >/dev/null 2>&1; then
  if command -v pkexec >/dev/null 2>&1; then
    pkexec deepin-immutable-ctl admin exec -- bash -c "$install_cmd"
  else
    deepin-immutable-ctl admin exec -- bash -c "$install_cmd"
  fi
  # 若包处于 iF（配置中断），用已修好的 postinst 收尾；timeout 防止再挂
  if dpkg -l deepin-video-wallpaper 2>/dev/null | grep -q '^iF'; then
    echo "包状态 iF，尝试 dpkg --configure（带超时）…"
    timeout 20 pkexec deepin-immutable-ctl admin exec -- dpkg --configure -a 2>/dev/null \
      || timeout 20 pkexec deepin-immutable-ctl admin exec -- dpkg --configure deepin-video-wallpaper 2>/dev/null \
      || true
  fi
  # 可选：登记 deb 版本（失败不挡使用）
  timeout 30 pkexec deepin-immutable-ctl admin exec -- dpkg -i --force-confnew "$DEB" 2>/dev/null || true
else
  sudo bash -c "$install_cmd"
  timeout 30 sudo dpkg -i "$DEB" || sudo apt -f install -y || true
fi

echo "==> [3/4] 用户目录备用 so"
mkdir -p "$HOME/.local/lib/dde-file-manager/plugins/desktop-edge"
cp -f "$SO_SRC" "$HOME/.local/lib/dde-file-manager/plugins/desktop-edge/libdd-videowallpaper-plugin.so"
ls -la "$HOME/.local/lib/dde-file-manager/plugins/desktop-edge/libdd-videowallpaper-plugin.so"

echo "==> [4/4] 用户会话内重启桌面（带超时，不卡死）"
timeout 15 systemctl --user restart 'dde-shell-plugin@org.deepin.ds.desktop.service' || true
sleep 2
echo "完成。so 与版本："
ls -la "$DEST/libdd-videowallpaper-plugin.so" 2>/dev/null || true
dpkg -l deepin-video-wallpaper 2>/dev/null | tail -1 || true
echo "桌面右键 → 动态壁纸设置。若画面仍旧，注销再登即可。"
