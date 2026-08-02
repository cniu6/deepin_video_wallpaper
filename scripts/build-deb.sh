#!/usr/bin/env bash
# 在 Deepin 本机一键打 deb（推荐；CI 也用同一脚本）
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

VERSION="${VERSION:-$(dpkg-parsechangelog -l debian/changelog -S Version 2>/dev/null || echo 1.0.0-1)}"
UPSTREAM_VERSION="${VERSION%%-*}"
ARCH="$(dpkg-architecture -qDEB_HOST_ARCH)"
OUT_DIR="${OUT_DIR:-$ROOT/dist}"
BUILD_DIR="$ROOT/build-deb"
STAGE="$BUILD_DIR/stage"
PLUGIN_DIR="usr/lib/$(dpkg-architecture -qDEB_HOST_MULTIARCH)/dde-file-manager/plugins/desktop-edge"

echo "==> 版本: $VERSION  架构: $ARCH"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR/cmake" "$STAGE/$PLUGIN_DIR" "$STAGE/usr/share/doc/deepin-video-wallpaper" "$OUT_DIR"

# ---- 编译 ----
cmake -S "$ROOT" -B "$BUILD_DIR/cmake" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DPLUGIN_INSTALL_DIR="lib/$(dpkg-architecture -qDEB_HOST_MULTIARCH)/dde-file-manager/plugins/desktop-edge"
cmake --build "$BUILD_DIR/cmake" -j"$(nproc)"

SO="$BUILD_DIR/cmake/libdd-videowallpaper-plugin.so"
if [[ ! -f "$SO" ]]; then
  echo "编译失败：找不到 $SO" >&2
  exit 1
fi

install -m 644 "$SO" "$STAGE/$PLUGIN_DIR/libdd-videowallpaper-plugin.so"
install -m 644 "$ROOT/README.md" "$STAGE/usr/share/doc/deepin-video-wallpaper/README.md" 2>/dev/null || true
install -m 644 "$ROOT/debian/copyright" "$STAGE/usr/share/doc/deepin-video-wallpaper/copyright"
gzip -n -9 -c "$ROOT/debian/changelog" > "$STAGE/usr/share/doc/deepin-video-wallpaper/changelog.Debian.gz"

# ---- DEBIAN 控制信息 ----
mkdir -p "$STAGE/DEBIAN"
# 用 dpkg-shlibdeps 生成 Depends（更准）
TMP_DEP="$BUILD_DIR/shlibdeps"
mkdir -p "$TMP_DEP"
# 伪造一个最小 control 给 shlibdeps
cat > "$TMP_DEP/control" <<EOF
Package: deepin-video-wallpaper
Version: $VERSION
Architecture: $ARCH
EOF
DEPENDS="libdde-file-manager, libavcodec60 | libavcodec, libavformat60 | libavformat, libavutil58 | libavutil, libswscale7 | libswscale, libqt6core6 | libqt6core6t64, libqt6gui6 | libqt6gui6t64, libqt6widgets6 | libqt6widgets6t64, libqt6dbus6 | libqt6dbus6t64, libdtk6core"
if command -v dpkg-shlibdeps >/dev/null 2>&1; then
  # 把 so 临时放到可分析路径
  mkdir -p "$TMP_DEP/lib"
  cp -a "$SO" "$TMP_DEP/lib/"
  set +e
  SHLIB=$(cd "$TMP_DEP" && dpkg-shlibdeps -O lib/libdd-videowallpaper-plugin.so 2>/dev/null | sed -n 's/^shlibs:Depends=//p')
  set -e
  if [[ -n "${SHLIB:-}" ]]; then
    DEPENDS="$SHLIB, libdde-file-manager"
  fi
fi

SIZE=$(du -sk "$STAGE" | awk '{print $1}')

cat > "$STAGE/DEBIAN/control" <<EOF
Package: deepin-video-wallpaper
Version: $VERSION
Architecture: $ARCH
Maintainer: cniu6 <cniu6@users.noreply.github.com>
Section: utils
Priority: optional
Homepage: https://github.com/cniu6/deepin_video_wallpaper
Installed-Size: $SIZE
Depends: $DEPENDS
Description: Deepin 动态视频壁纸插件
 作为 dde-shell 桌面 edge 插件运行，支持多屏与 CUDA/VAAPI 硬解。
 安装: sudo apt install ./deepin-video-wallpaper_*.deb
 卸载: sudo apt remove deepin-video-wallpaper
EOF

install -m 755 "$ROOT/debian/postinst" "$STAGE/DEBIAN/postinst"
install -m 755 "$ROOT/debian/prerm" "$STAGE/DEBIAN/prerm"
install -m 755 "$ROOT/debian/postrm" "$STAGE/DEBIAN/postrm"

# 去掉可能的 #DEBHELPER# 占位
sed -i 's/#DEBHELPER#//g' "$STAGE/DEBIAN/postinst" "$STAGE/DEBIAN/prerm" "$STAGE/DEBIAN/postrm"

DEB_NAME="deepin-video-wallpaper_${VERSION}_${ARCH}.deb"
dpkg-deb --root-owner-group --build "$STAGE" "$OUT_DIR/$DEB_NAME"

echo "==> 完成: $OUT_DIR/$DEB_NAME"
dpkg-deb -I "$OUT_DIR/$DEB_NAME"
echo "---- 内容 ----"
dpkg-deb -c "$OUT_DIR/$DEB_NAME"
