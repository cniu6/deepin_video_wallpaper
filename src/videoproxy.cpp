// SPDX-License-Identifier: GPL-3.0-or-later
#include "videoproxy.h"
#include "wallpaperconfig.h"

#include <QPainter>
#include <QPaintEvent>

using namespace ddplugin_videowallpaper;

VideoProxy::VideoProxy(QWidget *parent)
    : QWidget(parent)
{
    // 鼠标穿透：点击落到 canvas 图标
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_OpaquePaintEvent, true); // 整窗自绘，省合成
    setAutoFillBackground(false);
    paintGate.start();
}

VideoProxy::~VideoProxy()
{
    stop();
}

void VideoProxy::stop()
{
    pixmap = QPixmap();
    update();
}

void VideoProxy::updateImage(const QImage &img)
{
    if (img.isNull())
        return;
    const double fps = WpCfg->fps();
    // 跟片源：几乎不在 UI 侧再限帧（解码线程已按片源节奏出帧）
    // 指定 fps：按目标间隔门控，最少 1ms，支持 144/240
    qint64 interval = 1;
    if (fps > 0.0)
        interval = qMax<qint64>(1, qRound(1000.0 / fps));
    if (paintGate.isValid() && paintGate.elapsed() < interval)
        return;
    pixmap = QPixmap::fromImage(img);
    paintGate.restart();
    update();
}

void VideoProxy::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e)
    QPainter pa(this);
    pa.fillRect(rect(), Qt::black);
    if (pixmap.isNull())
        return;

    const FillMode mode = WpCfg->fillMode();
    const bool smooth = WpCfg->smoothLevel() != SmoothLevel::Fast;

    switch (mode) {
    case FillMode::Fit: {
        // 自适应：完整入屏，可能黑边
        const QSize tar = pixmap.size().scaled(size(), Qt::KeepAspectRatio);
        const int x = (width() - tar.width()) / 2;
        const int y = (height() - tar.height()) / 2;
        pa.setRenderHint(QPainter::SmoothPixmapTransform, smooth && tar != pixmap.size());
        pa.drawPixmap(QRect(x, y, tar.width(), tar.height()), pixmap);
        break;
    }
    case FillMode::Stretch: {
        // 拉伸：强制铺满，可能变形
        pa.setRenderHint(QPainter::SmoothPixmapTransform, smooth && size() != pixmap.size());
        pa.drawPixmap(rect(), pixmap);
        break;
    }
    case FillMode::Center: {
        // 居中：1:1 像素，不缩放（超出则裁，不足则黑边）
        const int x = (width() - pixmap.width()) / 2;
        const int y = (height() - pixmap.height()) / 2;
        pa.setRenderHint(QPainter::SmoothPixmapTransform, false);
        pa.drawPixmap(x, y, pixmap);
        break;
    }
    case FillMode::Tile: {
        // 平铺：重复铺满
        pa.setRenderHint(QPainter::SmoothPixmapTransform, false);
        pa.drawTiledPixmap(rect(), pixmap);
        break;
    }
    case FillMode::Fill:
    default: {
        // 铺满：等比放大裁切，无黑边
        const QSize tar = pixmap.size().scaled(size(), Qt::KeepAspectRatioByExpanding);
        const int x = (width() - tar.width()) / 2;
        const int y = (height() - tar.height()) / 2;
        pa.setRenderHint(QPainter::SmoothPixmapTransform, smooth && tar != pixmap.size());
        pa.drawPixmap(QRect(x, y, tar.width(), tar.height()), pixmap);
        break;
    }
    }
}
