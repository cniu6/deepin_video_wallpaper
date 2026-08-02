// SPDX-License-Identifier: GPL-3.0-or-later
#include "wallpaperengine_p.h"
#include "util/ddpugin_eventinterface_helper.h"
#include "wallpaperconfig.h"
#include "videowallpapermenuscene.h"
#include "util/menu_eventinterface_helper.h"

#include <dfm-base/dfm_desktop_defines.h>

#include <QDir>
#include <QStandardPaths>
#include <QDBusInterface>
#include <QDBusPendingReply>
#include <QFileInfo>
#include <QGuiApplication>
#include <QScreen>
#include <QTimer>
#include <QDebug>

using namespace ddplugin_videowallpaper;
DFMBASE_USE_NAMESPACE

#define CanvasCoreSubscribe(topic, func) \
    dpfSignalDispatcher->subscribe("ddplugin_core", QT_STRINGIFY2(topic), this, func)

#define CanvasCoreUnsubscribe(topic, func) \
    dpfSignalDispatcher->unsubscribe("ddplugin_core", QT_STRINGIFY2(topic), this, func)

static QString getScreenName(QWidget *win)
{
    return win->property(DesktopFrameProperty::kPropScreenName).toString();
}

static QMap<QString, QWidget *> rootMap()
{
    QList<QWidget *> root = ddplugin_desktop_util::desktopFrameRootWindows();
    QMap<QString, QWidget *> ret;
    for (QWidget *win : root) {
        QString name = getScreenName(win);
        if (name.isEmpty())
            continue;
        ret.insert(name, win);
    }
    return ret;
}

static QUrl firstVideoInDir(const QDir &dir)
{
    static const QStringList filters {
        QStringLiteral("*.mp4"), QStringLiteral("*.mkv"), QStringLiteral("*.webm"),
        QStringLiteral("*.avi"), QStringLiteral("*.mov"), QStringLiteral("*.m4v")
    };
    for (const QFileInfo &file : dir.entryInfoList(filters, QDir::Files | QDir::Readable, QDir::Name)) {
        if (file.fileName().compare(QStringLiteral("current.mp4"), Qt::CaseInsensitive) == 0)
            continue;
        const QString key = file.canonicalFilePath();
        if (!key.isEmpty())
            return QUrl::fromLocalFile(key);
    }
    return {};
}

WallpaperEnginePrivate::WallpaperEnginePrivate(WallpaperEngine *qq)
    : q(qq)
{
}

bool WallpaperEnginePrivate::isScreenActive(const QString &screenName) const
{
    return WpCfg->screenEnabled(screenName);
}

QUrl WallpaperEnginePrivate::videoForScreen(const QString &screenName) const
{
    const QString chosen = WpCfg->screenVideo(screenName).trimmed();
    if (!chosen.isEmpty()) {
        QFileInfo fi(chosen);
        if (fi.exists() && fi.isFile())
            return QUrl::fromLocalFile(fi.canonicalFilePath());
    }

    const QDir root(sourcePath());
    const QStringList candidates {
        root.filePath(QStringLiteral("screens/%1.mp4").arg(screenName)),
        root.filePath(QStringLiteral("%1.mp4").arg(screenName)),
    };
    for (const QString &p : candidates) {
        QFileInfo fi(p);
        if (fi.exists() && fi.isFile())
            return QUrl::fromLocalFile(fi.canonicalFilePath());
    }
    return firstVideoInDir(root);
}

int WallpaperEnginePrivate::maxScreenWidth() const
{
    int maxW = 0;
    for (QScreen *s : QGuiApplication::screens())
        maxW = qMax(maxW, s ? s->size().width() : 0);
    return maxW > 0 ? maxW : 1920;
}

PlayOptions WallpaperEnginePrivate::playOptions() const
{
    PlayOptions opt;
    opt.mode = WpCfg->decodeMode();
    opt.smooth = WpCfg->smoothLevel();
    opt.speed = WpCfg->speed();
    opt.fps = WpCfg->fps(); // 0=跟片源，不强制改成 30
    opt.maxWidth = WpCfg->maxWidth();
    // 0：按屏幕宽度；-1：源文件全分辨率
    if (opt.maxWidth == 0)
        opt.maxWidth = maxScreenWidth();
    else if (opt.maxWidth < 0)
        opt.maxWidth = -1;
    return opt;
}

VideoProxyPointer WallpaperEnginePrivate::createWidget(QWidget *root)
{
    VideoProxyPointer bwp(new VideoProxy());
    bwp->setParent(root);
    bwp->setGeometry(relativeGeometry(root->geometry()));
    const QString name = getScreenName(root);
    bwp->setProperty(DesktopFrameProperty::kPropScreenName, name);
    bwp->setProperty(DesktopFrameProperty::kPropWidgetName, "videowallpaper");
    bwp->setProperty(DesktopFrameProperty::kPropWidgetLevel, 5.1);
    return bwp;
}

void WallpaperEnginePrivate::setBackgroundVisible(bool v)
{
    for (QWidget *root : ddplugin_desktop_util::desktopFrameRootWindows()) {
        const QString name = getScreenName(root);
        if (v || isScreenActive(name))
            setBackgroundVisibleFor(name, v);
    }
}

void WallpaperEnginePrivate::setBackgroundVisibleFor(const QString &screenName, bool v)
{
    auto map = rootMap();
    QWidget *root = map.value(screenName);
    if (!root)
        return;
    for (QObject *obj : root->children()) {
        if (auto *wid = qobject_cast<QWidget *>(obj)) {
            if (wid->property(DesktopFrameProperty::kPropWidgetName).toString() == QLatin1String("background"))
                wid->setVisible(v);
        }
    }
}

QString WallpaperEnginePrivate::sourcePath() const
{
    const QString movies = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation)
            + QStringLiteral("/video-wallpaper");
    if (QDir(movies).exists())
        return movies;

    const QString homeVideos = QDir::homePath() + QStringLiteral("/Videos/video-wallpaper");
    if (QDir(homeVideos).exists())
        return homeVideos;

    return movies;
}

void WallpaperEnginePrivate::stopSharedDecoders()
{
    for (VideoDecoder *dec : decoders) {
        if (!dec)
            continue;
        dec->requestStop();
        dec->wait(4000);
        dec->deleteLater();
    }
    decoders.clear();
}

void WallpaperEnginePrivate::startSharedDecoders()
{
    stopSharedDecoders();
    PlayOptions popt = playOptions();
    DecodeOptions opt;
    opt.mode = popt.mode;
    opt.smooth = popt.smooth;
    opt.speed = popt.speed;
    opt.fps = popt.fps;
    opt.maxWidth = popt.maxWidth;

    QHash<QUrl, QList<QString>> urlScreens;
    for (auto it = screenVideo.constBegin(); it != screenVideo.constEnd(); ++it) {
        if (!isScreenActive(it.key()) || it.value().isEmpty())
            continue;
        urlScreens[it.value()].append(it.key());
    }

    for (auto it = urlScreens.begin(); it != urlScreens.end(); ++it) {
        auto *decoder = new VideoDecoder(q);
        decoder->setOptions(opt);
        decoder->setPlaylist({ it.key() });
        const QList<QString> screens = it.value();
        QObject::connect(decoder, &VideoDecoder::frameReady, q,
                         [this, screens](const QImage &img) {
            for (const QString &name : screens) {
                VideoProxyPointer w = widgets.value(name);
                if (!w.isNull())
                    w->updateImage(img);
            }
        }, Qt::QueuedConnection);
        decoder->start();
        decoders.insert(it.key(), decoder);
        qInfo() << "[videowallpaper] shared decoder" << it.key() << "screens" << screens
                << "maxW" << opt.maxWidth << "fps" << opt.fps;
    }
}

void WallpaperEnginePrivate::stopPlayers()
{
    if (startDebounce)
        startDebounce->stop();
    for (const VideoProxyPointer &w : widgets)
        if (!w.isNull())
            w->stop();
    stopSharedDecoders();
}

void WallpaperEnginePrivate::startPlayers()
{
    stopSharedDecoders();
    for (const VideoProxyPointer &w : widgets)
        if (!w.isNull())
            w->show();
    startSharedDecoders();
    for (auto it = screenVideo.begin(); it != screenVideo.end(); ++it) {
        if (isScreenActive(it.key()))
            setBackgroundVisibleFor(it.key(), false);
    }
}

WallpaperEngine::WallpaperEngine(QObject *parent)
    : QObject(parent)
    , d(new WallpaperEnginePrivate(this))
{
    d->startDebounce = new QTimer(this);
    d->startDebounce->setSingleShot(true);
    d->startDebounce->setInterval(300);
    connect(d->startDebounce, &QTimer::timeout, this, [this]() {
        if (WpCfg->enable())
            d->startPlayers();
    });
}

WallpaperEngine::~WallpaperEngine()
{
    turnOff();
    delete d;
    d = nullptr;
}

bool WallpaperEngine::init()
{
    try {
        WpCfg->initialize();

        QFileInfo source(d->sourcePath());
        if (!source.exists())
            source.absoluteDir().mkpath(source.fileName());
        QDir().mkpath(d->sourcePath() + QStringLiteral("/screens"));
        qInfo() << "[videowallpaper] resource dir:" << source.absoluteFilePath();

        if (!registerMenu())
            dpfSignalDispatcher->subscribe("dfmplugin_menu", "signal_MenuScene_SceneAdded",
                                           this, &WallpaperEngine::registerMenu);

        connect(WpCfg, &WallpaperConfig::checkResource, this, &WallpaperEngine::checkResouce);
        connect(WpCfg, &WallpaperConfig::changeEnableState, this, [this](bool e) {
            if (WpCfg->enable() != e)
                WpCfg->setEnable(e);
            if (e) {
                turnOn(true);
                play();
            } else {
                turnOff();
            }
        });
        connect(WpCfg, &WallpaperConfig::optionsChanged, this, &WallpaperEngine::onOptionsChanged);

        d->cfgWatcher = new QFileSystemWatcher(this);
        const QString cfgDir = QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation))
                .filePath(QStringLiteral("deepin-videowallpaper"));
        QDir().mkpath(cfgDir);
        d->cfgWatcher->addPath(cfgDir);
        connect(d->cfgWatcher, &QFileSystemWatcher::directoryChanged,
                this, &WallpaperEngine::onConfigFileChanged);
        connect(d->cfgWatcher, &QFileSystemWatcher::fileChanged,
                this, &WallpaperEngine::onConfigFileChanged);

        if (WpCfg->enable())
            turnOn(true);
    } catch (const std::exception &ex) {
        qWarning() << "[videowallpaper] init exception:" << ex.what();
        return false;
    } catch (...) {
        qWarning() << "[videowallpaper] init unknown exception";
        return false;
    }
    return true;
}

void WallpaperEngine::turnOn(bool b)
{
    if (d->watcher)
        return;

    CanvasCoreSubscribe(signal_DesktopFrame_WindowShowed, &WallpaperEngine::play);
    CanvasCoreSubscribe(signal_DesktopFrame_WindowBuilded, &WallpaperEngine::build);
    CanvasCoreSubscribe(signal_DesktopFrame_GeometryChanged, &WallpaperEngine::geometryChanged);
    CanvasCoreSubscribe(signal_DesktopFrame_WindowAboutToBeBuilded, &WallpaperEngine::onDetachWindows);

    d->watcher = new QFileSystemWatcher(this);
    const QString src = d->sourcePath();
    d->watcher->addPath(src);
    const QString screensDir = src + QStringLiteral("/screens");
    if (QDir(screensDir).exists())
        d->watcher->addPath(screensDir);
    connect(d->watcher, &QFileSystemWatcher::directoryChanged, this, &WallpaperEngine::refreshSource);

    refreshSource();
    if (b) {
        build();
        show();
    }
}

void WallpaperEngine::turnOff()
{
    CanvasCoreUnsubscribe(signal_DesktopFrame_WindowShowed, &WallpaperEngine::play);
    CanvasCoreUnsubscribe(signal_DesktopFrame_WindowBuilded, &WallpaperEngine::build);
    CanvasCoreUnsubscribe(signal_DesktopFrame_WindowAboutToBeBuilded, &WallpaperEngine::onDetachWindows);
    CanvasCoreUnsubscribe(signal_DesktopFrame_GeometryChanged, &WallpaperEngine::geometryChanged);

    delete d->watcher;
    d->watcher = nullptr;

    d->stopPlayers();
    d->widgets.clear();
    d->screenVideo.clear();
    d->setBackgroundVisible(true);
}

void WallpaperEngine::refreshSource()
{
    d->screenVideo.clear();
    for (QWidget *win : ddplugin_desktop_util::desktopFrameRootWindows()) {
        const QString name = getScreenName(win);
        if (name.isEmpty() || !d->isScreenActive(name))
            continue;
        const QUrl url = d->videoForScreen(name);
        if (!url.isEmpty())
            d->screenVideo.insert(name, url);
    }

    qInfo() << "[videowallpaper] screenVideo:" << d->screenVideo;
    if (WpCfg->enable())
        d->startDebounce->start();
}

void WallpaperEngine::build()
{
    QList<QWidget *> root = ddplugin_desktop_util::desktopFrameRootWindows();
    QMap<QString, QWidget *> alive;

    for (QWidget *win : root) {
        const QString screenName = getScreenName(win);
        if (screenName.isEmpty())
            continue;
        alive.insert(screenName, win);

        if (!d->isScreenActive(screenName)) {
            if (auto old = d->widgets.take(screenName))
                old->stop();
            d->screenVideo.remove(screenName);
            d->setBackgroundVisibleFor(screenName, true);
            continue;
        }

        VideoProxyPointer bwp = d->widgets.value(screenName);
        if (!bwp.isNull()) {
            bwp->setParent(win);
            bwp->setGeometry(d->relativeGeometry(win->geometry()));
        } else {
            bwp = d->createWidget(win);
            d->widgets.insert(screenName, bwp);
        }

        const QUrl url = d->videoForScreen(screenName);
        if (!url.isEmpty())
            d->screenVideo.insert(screenName, url);
    }

    for (const QString &sp : d->widgets.keys()) {
        if (!alive.contains(sp) || !d->isScreenActive(sp)) {
            if (auto old = d->widgets.take(sp))
                old->stop();
        }
    }

    if (WpCfg->enable())
        d->startDebounce->start();
}

void WallpaperEngine::onDetachWindows()
{
    for (const VideoProxyPointer &bwp : d->widgets.values()) {
        if (!bwp.isNull()) {
            bwp->stop();
            bwp->setParent(nullptr);
        }
    }
}

void WallpaperEngine::geometryChanged()
{
    build();
    auto winMap = rootMap();
    for (auto it = d->widgets.begin(); it != d->widgets.end(); ++it) {
        auto *win = winMap.value(it.key());
        if (!win || it.value().isNull())
            continue;
        it.value()->setGeometry(d->relativeGeometry(win->geometry()));
    }
    play();
}

void WallpaperEngine::play()
{
    if (!WpCfg->enable())
        return;

    show();

    for (QWidget *root : ddplugin_desktop_util::desktopFrameRootWindows()) {
        const QString name = getScreenName(root);
        const bool active = d->isScreenActive(name) && d->widgets.contains(name);
        if (!active) {
            d->setBackgroundVisibleFor(name, true);
            continue;
        }
        d->setBackgroundVisibleFor(name, false);
    }

    d->startDebounce->start();
}

void WallpaperEngine::show()
{
    dpfSlotChannel->push("ddplugin_core", "slot_DesktopFrame_LayoutWidget");
    for (const VideoProxyPointer &bwp : d->widgets.values()) {
        if (bwp.isNull())
            continue;
        bwp->show();
    }
}

bool WallpaperEngine::registerMenu()
{
    if (!dfmplugin_menu_util::menuSceneContains("CanvasMenu"))
        return false;

    dfmplugin_menu_util::menuSceneRegisterScene(VideoWallpaerMenuCreator::name(),
                                                new VideoWallpaerMenuCreator());
    dfmplugin_menu_util::menuSceneBind(VideoWallpaerMenuCreator::name(), "CanvasMenu");
    dpfSignalDispatcher->unsubscribe("dfmplugin_menu", "signal_MenuScene_SceneAdded",
                                     this, &WallpaperEngine::registerMenu);
    return true;
}

void WallpaperEngine::checkResouce()
{
    if (!d->screenVideo.isEmpty())
        return;

    const QString text = tr("Please add the video file to %0").arg(d->sourcePath());
    QDBusInterface notify("org.freedesktop.Notifications",
                          "/org/freedesktop/Notifications",
                          "org.freedesktop.Notifications");
    notify.setTimeout(1000);
    notify.asyncCall(QString("Notify"),
                     QString("Video Wallpaper"),
                     static_cast<uint>(0),
                     QString("deepin-toggle-desktop"),
                     text,
                     QString(), QStringList(), QVariantMap(), 5000);
}

void WallpaperEngine::catchImage(const QImage &img)
{
    Q_UNUSED(img)
}

void WallpaperEngine::onOptionsChanged()
{
    if (!WpCfg->enable())
        return;
    build();
    play();
}

void WallpaperEngine::onConfigFileChanged(const QString &)
{
    WpCfg->reload();
}
