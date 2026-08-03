// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef VIDEODECODER_H
#define VIDEODECODER_H

#include "wallpaperconfig.h"
#include "videoframe.h"

#include <QThread>
#include <QUrl>
#include <QMutex>
#include <QList>
#include <atomic>

namespace ddplugin_videowallpaper {

struct DecodeOptions {
    int maxWidth = -1;
    double fps = 0.0;
    double speed = 1.0;
    DecodeMode mode = DecodeMode::Auto;
    SmoothLevel smooth = SmoothLevel::High;
    bool preferNv12 = false;
};

class VideoDecoder : public QThread
{
    Q_OBJECT
public:
    static constexpr int kMaxInFlight = 2;

    explicit VideoDecoder(QObject *parent = nullptr);
    ~VideoDecoder() override;

    void setPlaylist(const QList<QUrl> &list);
    void setOptions(const DecodeOptions &opt);
    void requestStop();
    void releaseFrameSlot();

signals:
    void frameReady(const VideoFrame &frame);

protected:
    void run() override;

private:
    bool playOne(const QString &path);

    QList<QUrl> playlist;
    DecodeOptions options;
    QMutex mutex;
    std::atomic_bool stopFlag { false };
    std::atomic_int inFlight { 0 };
};

}

#endif
