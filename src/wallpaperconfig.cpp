// SPDX-License-Identifier: GPL-3.0-or-later
#include "wallpaperconfig_p.h"

#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

using namespace ddplugin_videowallpaper;

class WallpaperConfigGlobal : public WallpaperConfig {};
Q_GLOBAL_STATIC(WallpaperConfigGlobal, wallpaperConfig)

QString WallpaperConfig::decodeModeToString(DecodeMode m)
{
    switch (m) {
    case DecodeMode::Cuda: return QStringLiteral("cuda");
    case DecodeMode::Vaapi: return QStringLiteral("vaapi");
    case DecodeMode::Software: return QStringLiteral("software");
    case DecodeMode::Auto:
    default: return QStringLiteral("auto");
    }
}

DecodeMode WallpaperConfig::decodeModeFromString(const QString &s)
{
    const QString t = s.toLower();
    if (t == QLatin1String("cuda") || t == QLatin1String("nvdec") || t == QLatin1String("gpu"))
        return DecodeMode::Cuda;
    if (t == QLatin1String("vaapi") || t == QLatin1String("igpu"))
        return DecodeMode::Vaapi;
    if (t == QLatin1String("software") || t == QLatin1String("soft") || t == QLatin1String("cpu"))
        return DecodeMode::Software;
    return DecodeMode::Auto;
}

QString WallpaperConfig::smoothLevelToString(SmoothLevel m)
{
    switch (m) {
    case SmoothLevel::Fast: return QStringLiteral("fast");
    case SmoothLevel::Normal: return QStringLiteral("normal");
    case SmoothLevel::Highest: return QStringLiteral("highest");
    case SmoothLevel::High:
    default: return QStringLiteral("high");
    }
}

SmoothLevel WallpaperConfig::smoothLevelFromString(const QString &s)
{
    const QString t = s.toLower();
    if (t == QLatin1String("fast") || t == QLatin1String("0"))
        return SmoothLevel::Fast;
    if (t == QLatin1String("normal") || t == QLatin1String("1") || t == QLatin1String("medium"))
        return SmoothLevel::Normal;
    if (t == QLatin1String("highest") || t == QLatin1String("3") || t == QLatin1String("lanczos"))
        return SmoothLevel::Highest;
    return SmoothLevel::High;
}

QString WallpaperConfig::fillModeToString(FillMode m)
{
    switch (m) {
    case FillMode::Fit: return QStringLiteral("fit");
    case FillMode::Stretch: return QStringLiteral("stretch");
    case FillMode::Center: return QStringLiteral("center");
    case FillMode::Tile: return QStringLiteral("tile");
    case FillMode::Fill:
    default: return QStringLiteral("fill");
    }
}

FillMode WallpaperConfig::fillModeFromString(const QString &s)
{
    const QString t = s.toLower();
    if (t == QLatin1String("fit") || t == QLatin1String("contain") || t == QLatin1String("adapt"))
        return FillMode::Fit;
    if (t == QLatin1String("stretch") || t == QLatin1String("scale"))
        return FillMode::Stretch;
    if (t == QLatin1String("center") || t == QLatin1String("original"))
        return FillMode::Center;
    if (t == QLatin1String("tile") || t == QLatin1String("repeat"))
        return FillMode::Tile;
    // fill / cover / crop
    return FillMode::Fill;
}

QString WallpaperConfigPrivate::storePath()
{
    // 仅供程序/设置界面读写，不用手改
    return QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation))
            .filePath(QStringLiteral("deepin-videowallpaper/settings.json"));
}

WallpaperConfigPrivate::WallpaperConfigPrivate(WallpaperConfig *qq)
    : q(qq)
{
}

void WallpaperConfigPrivate::load()
{
    QFile f(storePath());
    if (!f.exists()) {
        // 兼容旧 ini：只迁移一次
        const QString oldIni = QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation))
                .filePath(QStringLiteral("deepin-videowallpaper/config.ini"));
        QFile ini(oldIni);
        if (ini.exists() && ini.open(QIODevice::ReadOnly)) {
            const QString text = QString::fromUtf8(ini.readAll());
            ini.close();
            enable = text.contains(QLatin1String("enable=true"));
        }
        return;
    }
    if (!f.open(QIODevice::ReadOnly))
        return;

    const auto doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject())
        return;

    const QJsonObject o = doc.object();
    enable = o.value(QStringLiteral("enable")).toBool(false);
    // fps=0 表示跟片源；其余限制在 1~240（支持 144/165/240 高刷）
    {
        const double v = o.value(QStringLiteral("fps")).toDouble(0.0);
        fps = (v <= 0.0) ? 0.0 : qBound(v, 1.0, 240.0);
    }
    speed = qBound(o.value(QStringLiteral("speed")).toDouble(1.0), 0.01, 4.0);
    // -1 允许（原始分辨率）
    maxWidth = o.value(QStringLiteral("maxWidth")).toInt(-1);
    if (maxWidth < -1)
        maxWidth = -1;
    if (maxWidth > 7680)
        maxWidth = 7680;
    // 缺省：CUDA / 全分辨率 / 原始帧率 / 平滑关 / 铺满
    decodeMode = WallpaperConfig::decodeModeFromString(
            o.value(QStringLiteral("decodeMode")).toString(QStringLiteral("cuda")));
    smoothLevel = WallpaperConfig::smoothLevelFromString(
            o.value(QStringLiteral("smoothLevel")).toString(QStringLiteral("fast")));
    fillMode = WallpaperConfig::fillModeFromString(
            o.value(QStringLiteral("fillMode")).toString(QStringLiteral("fill")));

    screens.clear();
    const QJsonObject so = o.value(QStringLiteral("screens")).toObject();
    for (auto it = so.begin(); it != so.end(); ++it) {
        const QJsonObject one = it.value().toObject();
        ScreenSetting ss;
        ss.enabled = one.value(QStringLiteral("enabled")).toBool(false);
        ss.video = one.value(QStringLiteral("video")).toString();
        screens.insert(it.key(), ss);
    }
}

void WallpaperConfigPrivate::store()
{
    QJsonObject o;
    o.insert(QStringLiteral("enable"), enable);
    o.insert(QStringLiteral("fps"), fps);
    o.insert(QStringLiteral("speed"), speed);
    o.insert(QStringLiteral("maxWidth"), maxWidth);
    o.insert(QStringLiteral("decodeMode"), WallpaperConfig::decodeModeToString(decodeMode));
    o.insert(QStringLiteral("smoothLevel"), WallpaperConfig::smoothLevelToString(smoothLevel));
    o.insert(QStringLiteral("fillMode"), WallpaperConfig::fillModeToString(fillMode));

    QJsonObject so;
    for (auto it = screens.constBegin(); it != screens.constEnd(); ++it) {
        QJsonObject one;
        one.insert(QStringLiteral("enabled"), it.value().enabled);
        one.insert(QStringLiteral("video"), it.value().video);
        so.insert(it.key(), one);
    }
    o.insert(QStringLiteral("screens"), so);

    const QString path = storePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "[videowallpaper] save settings failed:" << path;
        return;
    }
    f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
    f.close();
}

WallpaperConfig *WallpaperConfig::instance()
{
    return wallpaperConfig;
}

WallpaperConfig::WallpaperConfig(QObject *parent)
    : QObject(parent)
    , d(new WallpaperConfigPrivate(this))
{
}

void WallpaperConfig::initialize()
{
    d->load();
    qInfo() << "[videowallpaper] settings"
            << "enable=" << d->enable
            << "fps=" << d->fps
            << "maxWidth=" << d->maxWidth
            << "decode=" << decodeModeToString(d->decodeMode)
            << "fill=" << fillModeToString(d->fillMode)
            << "screens=" << d->screens.keys();
}

void WallpaperConfig::reload()
{
    const bool old = d->enable;
    d->load();
    emit optionsChanged();
    if (old != d->enable)
        emit changeEnableState(d->enable);
}

void WallpaperConfig::save()
{
    d->store();
    emit optionsChanged();
}

bool WallpaperConfig::enable() const { return d->enable; }
double WallpaperConfig::fps() const { return d->fps; }
double WallpaperConfig::speed() const { return d->speed; }
int WallpaperConfig::maxWidth() const { return d->maxWidth; }
DecodeMode WallpaperConfig::decodeMode() const { return d->decodeMode; }
SmoothLevel WallpaperConfig::smoothLevel() const { return d->smoothLevel; }
FillMode WallpaperConfig::fillMode() const { return d->fillMode; }
QHash<QString, ScreenSetting> WallpaperConfig::screenSettings() const { return d->screens; }

void WallpaperConfig::setEnable(bool e)
{
    if (d->enable == e)
        return;
    d->enable = e;
    d->store();
}

void WallpaperConfig::setFps(double v)
{
    // 0=跟片源；上限 240 支持高刷片源 / 高刷屏
    d->fps = (v <= 0.0) ? 0.0 : qBound(v, 1.0, 240.0);
}
void WallpaperConfig::setSpeed(double v) { d->speed = qBound(v, 0.01, 4.0); }
void WallpaperConfig::setMaxWidth(int v)
{
    if (v < -1)
        v = -1;
    if (v > 7680)
        v = 7680;
    d->maxWidth = v;
}
void WallpaperConfig::setSmoothLevel(SmoothLevel m) { d->smoothLevel = m; }
void WallpaperConfig::setFillMode(FillMode m) { d->fillMode = m; }
void WallpaperConfig::setDecodeMode(DecodeMode m) { d->decodeMode = m; }

void WallpaperConfig::setScreenSettings(const QHash<QString, ScreenSetting> &map)
{
    d->screens = map;
}

bool WallpaperConfig::screenEnabled(const QString &screenName) const
{
    // 尚未在界面配置过任何屏时：兼容旧行为，全部可播
    if (d->screens.isEmpty())
        return true;
    return d->screens.value(screenName).enabled;
}

QString WallpaperConfig::screenVideo(const QString &screenName) const
{
    return d->screens.value(screenName).video;
}

QStringList WallpaperConfig::enabledScreens() const
{
    QStringList ret;
    for (auto it = d->screens.constBegin(); it != d->screens.constEnd(); ++it) {
        if (it.value().enabled)
            ret << it.key();
    }
    return ret;
}
