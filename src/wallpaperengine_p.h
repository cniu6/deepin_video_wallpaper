// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef WALLPAPERENGINE_P_H
#define WALLPAPERENGINE_P_H

#include "wallpaperengine.h"
#include "videoproxy.h"
#include "videodecoder.h"

#include <QFileSystemWatcher>
#include <QHash>
#include <QUrl>

namespace ddplugin_videowallpaper {

class WallpaperEnginePrivate
{
public:
    explicit WallpaperEnginePrivate(WallpaperEngine *qq);
    inline QRect relativeGeometry(const QRect &geometry)
    {
        return QRect(QPoint(0, 0), geometry.size());
    }

    QUrl videoForScreen(const QString &screenName) const;
    VideoProxyPointer createWidget(QWidget *root);
    void setBackgroundVisible(bool v);
    void setBackgroundVisibleFor(const QString &screenName, bool v);
    QString sourcePath() const;
    void stopPlayers();
    void startPlayers();
    void startSharedDecoders();
    void stopSharedDecoders();
    bool isScreenActive(const QString &screenName) const;
    PlayOptions playOptions() const;
    int maxScreenWidth() const;

    QMap<QString, VideoProxyPointer> widgets;
    QHash<QString, QUrl> screenVideo;
    // 同一视频只解一份，多屏共享帧
    QHash<QUrl, VideoDecoder *> decoders;

    QFileSystemWatcher *watcher = nullptr;
    QFileSystemWatcher *cfgWatcher = nullptr;
    QTimer *startDebounce = nullptr;

private:
    WallpaperEngine *q;
};

}

#endif
