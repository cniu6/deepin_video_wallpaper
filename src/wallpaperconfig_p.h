// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef WALLPAPERCONFIG_P_H
#define WALLPAPERCONFIG_P_H

#include "wallpaperconfig.h"

namespace ddplugin_videowallpaper {

class WallpaperConfigPrivate
{
public:
    explicit WallpaperConfigPrivate(WallpaperConfig *qq);
    void load();
    void store();
    static QString storePath();

    bool enable = false;
    double fps = 0.0;          // 0=跟片源原始帧率；>0 限到 1~240
    double speed = 1.0;
    int maxWidth = -1;         // -1=源文件全分辨率；0=按屏幕；>0=上限
    DecodeMode decodeMode = DecodeMode::Software;       // 默认 CPU 软解；设置可切 CUDA/VAAPI
    SmoothLevel smoothLevel = SmoothLevel::Fast;    // 默认关掉平滑
    FillMode fillMode = FillMode::Fill;             // 默认铺满裁切
    bool showFps = true;                            // 默认显示实时 fps
    QHash<QString, ScreenSetting> screens;

private:
    WallpaperConfig *q;
};

}

#endif
