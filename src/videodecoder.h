// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef VIDEODECODER_H
#define VIDEODECODER_H

#include "wallpaperconfig.h"

#include <QThread>
#include <QImage>
#include <QUrl>
#include <QMutex>
#include <QList>
#include <atomic>

namespace ddplugin_videowallpaper {

struct DecodeOptions {
    int maxWidth = -1;         // -1=不降分辨率；0=上层会改成屏幕宽；>0=上限
    double fps = 0.0;          // 0=跟片源
    double speed = 1.0;
    DecodeMode mode = DecodeMode::Auto;
    SmoothLevel smooth = SmoothLevel::High;
};

class VideoDecoder : public QThread
{
    Q_OBJECT
public:
    explicit VideoDecoder(QObject *parent = nullptr);
    ~VideoDecoder() override;

    void setPlaylist(const QList<QUrl> &list);
    void setOptions(const DecodeOptions &opt);
    void requestStop();

signals:
    void frameReady(const QImage &img);

protected:
    void run() override;

private:
    bool playOne(const QString &path);
    QList<QUrl> playlist;
    DecodeOptions options;
    QMutex mutex;
    std::atomic_bool stopFlag { false };
};

}

#endif
