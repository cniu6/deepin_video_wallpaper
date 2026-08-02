// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef WALLPAPERENGINE_H
#define WALLPAPERENGINE_H

#include "ddplugin_videowallpaper_global.h"

#include <QObject>
#include <QImage>
#include <QUrl>

namespace ddplugin_videowallpaper {

class WallpaperEnginePrivate;
class WallpaperEngine : public QObject
{
    Q_OBJECT
    friend class WallpaperEnginePrivate;
public:
    explicit WallpaperEngine(QObject *parent = nullptr);
    ~WallpaperEngine() override;
    bool init();
    void turnOn(bool build = true);
    void turnOff();

public slots:
    void refreshSource();
    void build();
    void onDetachWindows();
    void geometryChanged();
    void play();
    void show();

private slots:
    bool registerMenu();
    void checkResouce();
    void catchImage(const QImage &img);
    void onOptionsChanged();
    void onConfigFileChanged(const QString &path);

private:
    WallpaperEnginePrivate *d;
};

}

#endif
