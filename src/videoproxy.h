// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef VIDEOPROXY_H
#define VIDEOPROXY_H

#include "ddplugin_videowallpaper_global.h"
#include "wallpaperconfig.h"

#include <QWidget>
#include <QPixmap>
#include <QElapsedTimer>
#include <QSharedPointer>

namespace ddplugin_videowallpaper {

struct PlayOptions {
    DecodeMode mode = DecodeMode::Cuda;
    SmoothLevel smooth = SmoothLevel::Fast;
    FillMode fill = FillMode::Fill;
    double speed = 1.0;
    double fps = 0.0;   // 0=跟片源；1~240
    int maxWidth = -1;
};

class VideoProxy : public QWidget
{
    Q_OBJECT
public:
    explicit VideoProxy(QWidget *parent = nullptr);
    ~VideoProxy() override;

    void stop();
    void updateImage(const QImage &img);
    /** 主线程已转好的 pixmap（多屏共享，避免每屏 fromImage 一次） */
    void updatePixmap(const QPixmap &pm, int srcW, int srcH);
    /** 仅刷新叠层（开关 showFps 时用，不必重解） */
    void refreshOverlay();

protected:
    void paintEvent(QPaintEvent *) override;

private:
    void drawFpsOverlay(QPainter &pa);
    void notePresented(int srcW, int srcH);

    QPixmap pixmap;
    QElapsedTimer paintGate;
    QElapsedTimer fpsClock;
    qint64 nextPaintUs = 0;
    int framesInWindow = 0;
    double displayFps = 0.0;
    int lastFrameW = 0;
    int lastFrameH = 0;
};

typedef QSharedPointer<VideoProxy> VideoProxyPointer;

}

#endif
