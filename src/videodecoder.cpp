// SPDX-License-Identifier: GPL-3.0-or-later
#include "videodecoder.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QThread>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/hwcontext.h>
#include <libswscale/swscale.h>
}

using namespace ddplugin_videowallpaper;

static enum AVPixelFormat s_hwPixFmt = AV_PIX_FMT_NONE;

static enum AVPixelFormat getHwFormat(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts)
{
    Q_UNUSED(ctx)
    for (const enum AVPixelFormat *p = pix_fmts; *p != -1; ++p) {
        if (*p == s_hwPixFmt)
            return *p;
    }
    return pix_fmts[0];
}

static AVBufferRef *createHwDevice(DecodeMode mode, AVHWDeviceType *outType)
{
    QList<AVHWDeviceType> candidates;
    switch (mode) {
    case DecodeMode::Cuda:
        candidates << AV_HWDEVICE_TYPE_CUDA;
        break;
    case DecodeMode::Vaapi:
        candidates << AV_HWDEVICE_TYPE_VAAPI;
        break;
    case DecodeMode::Software:
        *outType = AV_HWDEVICE_TYPE_NONE;
        return nullptr;
    case DecodeMode::Auto:
    default:
        // 独显 CUDA → 核显 VAAPI
        candidates << AV_HWDEVICE_TYPE_CUDA << AV_HWDEVICE_TYPE_VAAPI;
        break;
    }

    for (AVHWDeviceType t : candidates) {
        AVBufferRef *dev = nullptr;
        if (av_hwdevice_ctx_create(&dev, t, nullptr, nullptr, 0) == 0) {
            *outType = t;
            return dev;
        }
    }
    *outType = AV_HWDEVICE_TYPE_NONE;
    return nullptr;
}

VideoDecoder::VideoDecoder(QObject *parent)
    : QThread(parent)
{
}

VideoDecoder::~VideoDecoder()
{
    requestStop();
    wait(3000);
}

void VideoDecoder::setPlaylist(const QList<QUrl> &list)
{
    QMutexLocker locker(&mutex);
    playlist = list;
}

void VideoDecoder::setOptions(const DecodeOptions &opt)
{
    QMutexLocker locker(&mutex);
    options = opt;
}

void VideoDecoder::requestStop()
{
    stopFlag.store(true);
}

bool VideoDecoder::playOne(const QString &path)
{
    DecodeOptions opt;
    {
        QMutexLocker locker(&mutex);
        opt = options;
    }

    AVFormatContext *fmt = nullptr;
    if (avformat_open_input(&fmt, path.toUtf8().constData(), nullptr, nullptr) < 0) {
        qWarning() << "[videowallpaper] open failed:" << path;
        return false;
    }
    if (avformat_find_stream_info(fmt, nullptr) < 0) {
        avformat_close_input(&fmt);
        return false;
    }

    int vIndex = av_find_best_stream(fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (vIndex < 0) {
        avformat_close_input(&fmt);
        return false;
    }

    AVStream *st = fmt->streams[vIndex];
    const AVCodec *codec = avcodec_find_decoder(st->codecpar->codec_id);
    if (!codec) {
        avformat_close_input(&fmt);
        return false;
    }

    AVCodecContext *ctx = avcodec_alloc_context3(codec);
    if (!ctx) {
        avformat_close_input(&fmt);
        return false;
    }
    avcodec_parameters_to_context(ctx, st->codecpar);

    AVHWDeviceType hwType = AV_HWDEVICE_TYPE_NONE;
    AVBufferRef *hwDev = createHwDevice(opt.mode, &hwType);
    bool useHw = false;
    if (hwDev) {
        for (int i = 0;; ++i) {
            const AVCodecHWConfig *cfg = avcodec_get_hw_config(codec, i);
            if (!cfg)
                break;
            if ((cfg->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)
                && cfg->device_type == hwType) {
                s_hwPixFmt = cfg->pix_fmt;
                ctx->hw_device_ctx = av_buffer_ref(hwDev);
                ctx->get_format = getHwFormat;
                useHw = true;
                break;
            }
        }
        if (!useHw) {
            av_buffer_unref(&hwDev);
            hwDev = nullptr;
        }
    }

    ctx->thread_count = 1;
    ctx->thread_type = FF_THREAD_SLICE;

    if (avcodec_open2(ctx, codec, nullptr) < 0) {
        if (useHw) {
            av_buffer_unref(&ctx->hw_device_ctx);
            ctx->hw_device_ctx = nullptr;
            ctx->get_format = nullptr;
            useHw = false;
            av_buffer_unref(&hwDev);
            hwDev = nullptr;
            if (avcodec_open2(ctx, codec, nullptr) < 0) {
                avcodec_free_context(&ctx);
                avformat_close_input(&fmt);
                return false;
            }
        } else {
            avcodec_free_context(&ctx);
            avformat_close_input(&fmt);
            return false;
        }
    }

    int srcW = ctx->width > 0 ? ctx->width : st->codecpar->width;
    int srcH = ctx->height > 0 ? ctx->height : st->codecpar->height;
    int dstW = srcW;
    int dstH = srcH;
    // maxWidth<0：不降分辨率；>0：超过才缩小；0 由上层填成屏幕宽
    if (opt.maxWidth > 0 && dstW > opt.maxWidth) {
        dstH = qMax(1, srcH * opt.maxWidth / srcW);
        dstW = opt.maxWidth;
    }

    qInfo() << "[videowallpaper] decode" << path
            << "hw=" << useHw
            << "type=" << (useHw ? av_hwdevice_get_type_name(hwType) : "software")
            << "size=" << dstW << "x" << dstH
            << "fps=" << opt.fps
            << "speed=" << opt.speed;

    AVFrame *frame = av_frame_alloc();
    AVFrame *swFrame = av_frame_alloc();
    AVFrame *rgb = av_frame_alloc();
    AVPacket *pkt = av_packet_alloc();
    QByteArray buf;
    buf.resize(av_image_get_buffer_size(AV_PIX_FMT_RGB32, dstW, dstH, 1));
    av_image_fill_arrays(rgb->data, rgb->linesize,
                         reinterpret_cast<uint8_t *>(buf.data()),
                         AV_PIX_FMT_RGB32, dstW, dstH, 1);

    SwsContext *sws = nullptr;
    AVPixelFormat swsFmt = AV_PIX_FMT_NONE;
    int swsW = 0, swsH = 0;
    auto ensureSws = [&](enum AVPixelFormat srcFmt, int w, int h) -> bool {
        if (sws && swsFmt == srcFmt && swsW == w && swsH == h)
            return true;
        if (sws) {
            sws_freeContext(sws);
            sws = nullptr;
        }
        // 按平滑等级选 swscale 算法；1:1 用 Point
        int flags = SWS_POINT;
        if (w != dstW || h != dstH) {
            switch (opt.smooth) {
            case SmoothLevel::Fast: flags = SWS_FAST_BILINEAR; break;
            case SmoothLevel::Normal: flags = SWS_BILINEAR; break;
            case SmoothLevel::Highest: flags = SWS_LANCZOS; break;
            case SmoothLevel::High:
            default: flags = SWS_BICUBIC; break;
            }
        }
        sws = sws_getContext(w, h, srcFmt, dstW, dstH, AV_PIX_FMT_RGB32,
                             flags, nullptr, nullptr, nullptr);
        if (!sws)
            return false;
        swsFmt = srcFmt;
        swsW = w;
        swsH = h;
        return true;
    };

    // 片源帧率：优先 avg，不行再用 r_frame_rate；支持 120/144/240
    double srcFps = av_q2d(st->avg_frame_rate);
    if (srcFps < 1.0 || srcFps > 240.0)
        srcFps = av_q2d(st->r_frame_rate);
    if (srcFps < 1.0 || srcFps > 240.0)
        srcFps = 30.0;
    // fps<=0：跟片源；>0：限到 1~240（不再卡死 60）
    const double targetFps = (opt.fps <= 0.0) ? srcFps : qBound(opt.fps, 1.0, 240.0);
    const double speed = qBound(opt.speed, 0.01, 4.0);
    // 目标低于片源时跳帧；目标≥片源则每帧都出
    const int skipMod = qMax(1, qRound(srcFps / qMax(1.0, targetFps)));
    // 最小 1ms，才能跑到约 1000fps 理论上限；144fps ≈ 7ms
    const qint64 frameIntervalMs = qMax<qint64>(1, qRound(1000.0 / (targetFps * speed)));

    QElapsedTimer timer;
    timer.start();
    qint64 nextPts = 0;
    int frameCount = 0;

    while (!stopFlag.load()) {
        int ret = av_read_frame(fmt, pkt);
        if (ret < 0) {
            av_seek_frame(fmt, vIndex, 0, AVSEEK_FLAG_BACKWARD);
            avcodec_flush_buffers(ctx);
            frameCount = 0;
            continue;
        }
        if (pkt->stream_index != vIndex) {
            av_packet_unref(pkt);
            continue;
        }
        if (avcodec_send_packet(ctx, pkt) < 0) {
            av_packet_unref(pkt);
            continue;
        }
        av_packet_unref(pkt);

        while (!stopFlag.load() && avcodec_receive_frame(ctx, frame) == 0) {
            ++frameCount;
            if ((frameCount % skipMod) != 0)
                continue;

            AVFrame *src = frame;
            if (useHw && frame->format == s_hwPixFmt) {
                if (av_hwframe_transfer_data(swFrame, frame, 0) < 0)
                    continue;
                src = swFrame;
            }

            const int w = src->width > 0 ? src->width : srcW;
            const int h = src->height > 0 ? src->height : srcH;
            if (!ensureSws(static_cast<AVPixelFormat>(src->format), w, h))
                continue;

            sws_scale(sws, src->data, src->linesize, 0, h, rgb->data, rgb->linesize);
            QImage img(rgb->data[0], dstW, dstH, rgb->linesize[0], QImage::Format_RGB32);
            emit frameReady(img.copy());

            nextPts += frameIntervalMs;
            const qint64 delay = nextPts - timer.elapsed();
            // 低倍速时间隔会很长，允许最多睡 5 秒一段
            if (delay > 0)
                QThread::msleep(static_cast<unsigned long>(qMin<qint64>(delay, 5000)));
            else if (delay < -800)
                nextPts = timer.elapsed();
        }
    }

    if (sws)
        sws_freeContext(sws);
    av_packet_free(&pkt);
    av_frame_free(&rgb);
    av_frame_free(&swFrame);
    av_frame_free(&frame);
    avcodec_free_context(&ctx);
    av_buffer_unref(&hwDev);
    avformat_close_input(&fmt);
    return true;
}

void VideoDecoder::run()
{
    stopFlag.store(false);
    while (!stopFlag.load()) {
        QList<QUrl> list;
        {
            QMutexLocker locker(&mutex);
            list = playlist;
        }
        if (list.isEmpty()) {
            QThread::msleep(500);
            continue;
        }
        for (const QUrl &url : list) {
            if (stopFlag.load())
                break;
            if (!url.isLocalFile())
                continue;
            playOne(url.toLocalFile());
        }
    }
}
