// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef WALLPAPERCONFIG_H
#define WALLPAPERCONFIG_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QHash>

namespace ddplugin_videowallpaper {

enum class DecodeMode {
    Auto = 0,      // 优先独显 CUDA，再核显 VAAPI，最后软解
    Cuda,          // NVIDIA 硬解
    Vaapi,         // 核显/通用硬解
    Software       // 软解
};

// 缩放/抗锯齿平滑等级（解码 swscale + 绘制）
enum class SmoothLevel {
    Fast = 0,      // 最快，锯齿多
    Normal,        // 均衡
    High,          // 更平滑
    Highest        // 最细（更吃 CPU）
};

struct ScreenSetting {
    bool enabled = false;
    QString video; // 空=用目录默认 current.mp4
};

class WallpaperConfigPrivate;
class WallpaperConfig : public QObject
{
    Q_OBJECT
    friend class WallpaperConfigPrivate;
public:
    static WallpaperConfig *instance();
    void initialize();
    void reload();
    void save();

    bool enable() const;
    void setEnable(bool);

    double fps() const;
    void setFps(double);

    double speed() const;
    void setSpeed(double);

    // -1=不降分辨率；0=按屏幕宽；>0=最大宽度
    int maxWidth() const;
    void setMaxWidth(int);

    DecodeMode decodeMode() const;
    void setDecodeMode(DecodeMode);

    SmoothLevel smoothLevel() const;
    void setSmoothLevel(SmoothLevel);

    QHash<QString, ScreenSetting> screenSettings() const;
    void setScreenSettings(const QHash<QString, ScreenSetting> &map);

    bool screenEnabled(const QString &screenName) const;
    QString screenVideo(const QString &screenName) const;
    QStringList enabledScreens() const;

    static QString decodeModeToString(DecodeMode m);
    static DecodeMode decodeModeFromString(const QString &s);
    static QString smoothLevelToString(SmoothLevel m);
    static SmoothLevel smoothLevelFromString(const QString &s);

signals:
    void changeEnableState(bool enable);
    void checkResource();
    void optionsChanged();

protected:
    explicit WallpaperConfig(QObject *parent = nullptr);

private:
    WallpaperConfigPrivate *d;
};

}

#define WpCfg WallpaperConfig::instance()

#endif
