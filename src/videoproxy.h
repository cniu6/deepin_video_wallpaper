// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef VIDEOPROXY_H
#define VIDEOPROXY_H

#include "ddplugin_videowallpaper_global.h"
#include "wallpaperconfig.h"
#include "videoframe.h"

#include <QWidget>
#include <QPixmap>
#include <QImage>
#include <QElapsedTimer>
#include <QSharedPointer>
#include <functional>

namespace ddplugin_videowallpaper {

struct PlayOptions {
    DecodeMode mode = DecodeMode::Software;
    SmoothLevel smooth = SmoothLevel::Fast;
    FillMode fill = FillMode::Fill;
    double speed = 1.0;
    double fps = 0.0;
    int maxWidth = -1;
};

/**
 * 嵌入桌面 root 的 QWidget（禁止 QOpenGLWidget / 独立窗）。
 * 呈现用 QPixmap + drawPixmap：X11 上比每帧 drawImage 更贴合成路径。
 */
class VideoProxy : public QWidget
{
    Q_OBJECT
public:
    explicit VideoProxy(QWidget *parent = nullptr);
    ~VideoProxy() override;

    void stop();
    void updateImage(const QImage &img);
    /** 多屏共享同一 QPixmap（主线程只 fromImage 一次） */
    void presentPixmap(const QPixmap &pm, int srcW, int srcH,
                       const std::function<void()> &painted = {});
    void present(const VideoFrame &frame, const std::function<void()> &painted = {});
    void refreshOverlay();

protected:
    void paintEvent(QPaintEvent *) override;

private:
    void drawFpsOverlay(QPainter &pa);
    void armPaint(int srcW, int srcH);

    QPixmap pixmap;
    QElapsedTimer paintGate;
    QElapsedTimer fpsClock;
    qint64 nextPaintUs = 0;
    int framesInWindow = 0;
    double displayFps = 0.0;
    int lastFrameW = 0;
    int lastFrameH = 0;
    bool paintScheduled = false;
    std::function<void()> afterPaint;
};

typedef QSharedPointer<VideoProxy> VideoProxyPointer;

}

#endif
