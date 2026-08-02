# Deepin 动态视频壁纸

Deepin 25 桌面 **edge 插件**：多屏视频壁纸，FFmpeg 共享解码，支持 NVIDIA CUDA / VAAPI 硬解。

仓库：<https://github.com/cniu6/deepin_video_wallpaper>

## 安装（deb）

从 [Releases](https://github.com/cniu6/deepin_video_wallpaper/releases) 下载 `.deb`：

```bash
sudo apt install ./deepin-video-wallpaper_*.deb
# 或
sudo dpkg -i deepin-video-wallpaper_*.deb
sudo apt -f install   # 若缺依赖
```

也可用深度商店 / `apt` 图形卸载界面管理（包名：`deepin-video-wallpaper`）。

## 卸载

```bash
sudo apt remove deepin-video-wallpaper
# 彻底删包（配置仍保留在用户目录）
sudo apt purge deepin-video-wallpaper
```

用户配置（可选手动删）：

```bash
rm -rf ~/.config/deepin-videowallpaper
```

## 使用

1. 把视频放到 `~/Videos/video-wallpaper/`
2. 桌面空白处 **右键 → 动态壁纸设置**
3. 勾选启用、选屏、选视频，点应用

默认：CUDA 硬解、源文件全分辨率、原始帧率、平滑关闭。

未生效时重启桌面插件：

```bash
systemctl --user restart 'dde-shell-plugin@org.deepin.ds.desktop.service'
```

## 本地打包

在 **Deepin 25** 上：

```bash
./scripts/build-deb.sh
# 产物：dist/deepin-video-wallpaper_1.0.0-1_amd64.deb
```

或用 debhelper：

```bash
dpkg-buildpackage -us -uc -b
```

## GitHub 自动 Release

1. 推送代码到 `main`
2. 打 tag 并推送：

```bash
git tag v1.0.0
git push origin main
git push origin v1.0.0
```

Actions 会编译 deb 并发到 Releases。

> **注意**：插件依赖 Deepin 的 `libdde-file-manager` / DTK，GitHub 默认 Ubuntu Runner 往往编不过。  
> 推荐：在本机 Deepin 装 [self-hosted runner](https://docs.github.com/en/actions/hosting-your-own-runners)，或本机 `./scripts/build-deb.sh` 后手动上传 Release。  
> 仓库 Variables 可设 `RUNNER_LABEL=self-hosted` 指定自托管。

## 开发编译（不装 deb）

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
# 手动安装 so 到插件目录（不可变系统需 deepin-immutable-ctl）
```

## License

GPL-3.0-or-later
