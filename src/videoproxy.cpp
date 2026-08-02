// SPDX-License-Identifier: GPL-3.0-or-later
#include "videoproxy.h"
#include "wallpaperconfig.h"

#include <QPainter>
#include <QPaintEvent>
#include <QFont>

using namespace ddplugin_videowallpaper;

VideoProxy::VideoProxy(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAutoFillBackground(false);
    paintGate.start();
    fpsClock.start();
}

VideoProxy::~VideoProxy()
{
    stop();
}

void VideoProxy::stop()
{
    pixmap = QPixmap();
    framesInWindow = 0;
    displayFps = 0.0;
    lastFrameW = lastFrameH = 0;
    nextPaintUs = 0;
    update();
}

void VideoProxy::refreshOverlay()
{
    update();
}

void VideoProxy::notePresented(int srcW, int srcH)
{
    lastFrameW = srcW;
    lastFrameH = srcH;
    ++framesInWindow;
    const qint64 elapsed = fpsClock.elapsed();
    // 用约 1s 窗口，读数更稳，少抖
    if (elapsed >= 1000) {
        displayFps = framesInWindow * 1000.0 / double(elapsed);
        framesInWindow = 0;
        fpsClock.restart();
    }
    update();
}

void VideoProxy::updatePixmap(const QPixmap &pm, int srcW, int srcH)
{
    if (pm.isNull())
        return;
    pixmap = pm;
    notePresented(srcW, srcH);
}

void VideoProxy::updateImage(const QImage &img)
{
    if (img.isNull())
        return;

    // 指定帧率时用微秒门控；原始(0) 完全跟解码，不再二次卡死
    const double cfgFps = WpCfg->fps();
    if (cfgFps > 0.0) {
        if (!paintGate.isValid())
            paintGate.start();
        const qint64 minGapUs = qMax<qint64>(1, qRound(1000000.0 / cfgFps));
        const qint64 now = paintGate.nsecsElapsed() / 1000;
        if (now < nextPaintUs)
            return;
        nextPaintUs = now + minGapUs;
    }

    pixmap = QPixmap::fromImage(img);
    notePresented(img.width(), img.height());
}

void VideoProxy::drawFpsOverlay(QPainter &pa)
{
    // 设置里的开关：关则完全不画
    if (!WpCfg->showFps())
        return;

    const double cfg = WpCfg->fps();
    const QString target = (cfg <= 0.0)
            ? QStringLiteral("设置:原始")
            : QStringLiteral("设置:%1").arg(qRound(cfg));

    // 实际 | 设置目标 | 分辨率
    const QString text = QStringLiteral("%1 fps | %2 | %3x%4")
            .arg(displayFps, 0, 'f', 1)
            .arg(target)
            .arg(lastFrameW)
            .arg(lastFrameH);

    QFont font = pa.font();
    font.setPointSize(13);
    font.setBold(true);
    pa.setFont(font);

    const QFontMetrics fm(font);
    const int pad = 8;
    const QRect box(12, 12,
                    fm.horizontalAdvance(text) + pad * 2,
                    fm.height() + pad * 2);

    pa.setPen(Qt::NoPen);
    pa.setBrush(QColor(0, 0, 0, 170));
    pa.drawRoundedRect(box, 6, 6);
    pa.setPen(QColor(0, 255, 120));
    pa.drawText(box, Qt::AlignCenter, text);
}

void VideoProxy::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e)
    QPainter pa(this);
    pa.fillRect(rect(), Qt::black);
    if (pixmap.isNull()) {
        drawFpsOverlay(pa);
        return;
    }

    const FillMode mode = WpCfg->fillMode();
    const bool smooth = (WpCfg->smoothLevel() == SmoothLevel::High
                         || WpCfg->smoothLevel() == SmoothLevel::Highest);

    switch (mode) {
    case FillMode::Fit: {
        const QSize tar = pixmap.size().scaled(size(), Qt::KeepAspectRatio);
        const int x = (width() - tar.width()) / 2;
        const int y = (height() - tar.height()) / 2;
        pa.setRenderHint(QPainter::SmoothPixmapTransform, smooth && tar != pixmap.size());
        pa.drawPixmap(QRect(x, y, tar.width(), tar.height()), pixmap);
        break;
    }
    case FillMode::Stretch: {
        pa.setRenderHint(QPainter::SmoothPixmapTransform, smooth && size() != pixmap.size());
        pa.drawPixmap(rect(), pixmap);
        break;
    }
    case FillMode::Center: {
        const int x = (width() - pixmap.width()) / 2;
        const int y = (height() - pixmap.height()) / 2;
        pa.setRenderHint(QPainter::SmoothPixmapTransform, false);
        pa.drawPixmap(x, y, pixmap);
        break;
    }
    case FillMode::Tile: {
        pa.setRenderHint(QPainter::SmoothPixmapTransform, false);
        pa.drawTiledPixmap(rect(), pixmap);
        break;
    }
    case FillMode::Fill:
    default: {
        const QSize tar = pixmap.size().scaled(size(), Qt::KeepAspectRatioByExpanding);
        const int x = (width() - tar.width()) / 2;
        const int y = (height() - tar.height()) / 2;
        pa.setRenderHint(QPainter::SmoothPixmapTransform, smooth && tar != pixmap.size());
        pa.drawPixmap(QRect(x, y, tar.width(), tar.height()), pixmap);
        break;
    }
    }

    drawFpsOverlay(pa);
}
