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
    // fps=0 跟片源：按解码侧节奏来，这里只做最小间隔防刷爆
    const qint64 interval = (fps > 0.0)
            ? qMax<qint64>(16, qRound(1000.0 / fps))
            : minPaintIntervalMs;
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
    const QSize tar = pixmap.size().scaled(size(), Qt::KeepAspectRatioByExpanding);
    const int x = (width() - tar.width()) / 2;
    const int y = (height() - tar.height()) / 2;
    // Fast = 关闭平滑；其余等级开平滑缩放
    pa.setRenderHint(QPainter::SmoothPixmapTransform,
                     tar != pixmap.size() && WpCfg->smoothLevel() != SmoothLevel::Fast);
    pa.drawPixmap(QRect(x, y, tar.width(), tar.height()), pixmap);
}
