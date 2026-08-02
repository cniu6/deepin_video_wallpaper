// SPDX-License-Identifier: GPL-3.0-or-later
#include "videowallpaperplugin.h"
#include "wallpaperengine.h"

#include <QDebug>

namespace ddplugin_videowallpaper {
DFM_LOG_REISGER_CATEGORY(DDP_VIDEOWALLPAPER_NAMESPACE)
}

DDP_VIDEOWALLPAPER_USE_NAMESPACE

VideoWallpaperPlugin::VideoWallpaperPlugin(QObject *parent)
    : Plugin()
{
    Q_UNUSED(parent)
}

void VideoWallpaperPlugin::initialize()
{
    // 安全版：不用 libdmr，不碰 locale / OpenGL
}

bool VideoWallpaperPlugin::start()
{
    try {
        engine = new WallpaperEngine();
        return engine->init();
    } catch (...) {
        qWarning() << "[videowallpaper] plugin start failed";
        delete engine;
        engine = nullptr;
        return false;
    }
}

void VideoWallpaperPlugin::stop()
{
    delete engine;
    engine = nullptr;
}
