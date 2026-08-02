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
    int maxWidth = -1;  // -1=源文件全分辨率
};

/** 软路径自绘：接收解码帧，QPainter 画到桌面 videowallpaper 层。 */
class VideoProxy : public QWidget
{
    Q_OBJECT
public:
    explicit VideoProxy(QWidget *parent = nullptr);
    ~VideoProxy() override;

    void stop();
    void updateImage(const QImage &img);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QPixmap pixmap;
    QElapsedTimer paintGate;
};

typedef QSharedPointer<VideoProxy> VideoProxyPointer;

}

#endif
