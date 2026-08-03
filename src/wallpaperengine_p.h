// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef WALLPAPERENGINE_P_H
#define WALLPAPERENGINE_P_H

#include "wallpaperengine.h"
#include "videoproxy.h"
#include "videodecoder.h"

#include <QFileSystemWatcher>
#include <QHash>
#include <QUrl>
#include <QTimer>

class QDBusInterface;

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
    /** 仅统计正在播放该 url 的屏控件物理宽，避免双屏被最大屏拖到无意义的超宽 sws */
    int maxWidthForScreens(const QList<QString> &screens) const;
    int maxScreenWidth() const;

    void setPlaybackSuspended(bool suspended, const char *reason);
    void setupPowerHooks();
    bool playbackSuspended = false;
    bool sessionLocked = false;
    bool screenSaverActive = false;

    QMap<QString, VideoProxyPointer> widgets;
    QHash<QString, QUrl> screenVideo;
    QHash<QUrl, VideoDecoder *> decoders;

    QFileSystemWatcher *watcher = nullptr;
    QFileSystemWatcher *cfgWatcher = nullptr;
    QTimer *startDebounce = nullptr;
    QTimer *visibilityTimer = nullptr;

private:
    WallpaperEngine *q;
};

}

#endif
