// SPDX-License-Identifier: GPL-3.0-or-later
#include "ddplugin_videowallpaper_global.h"
#include "videowallpapermenuscene.h"
#include "wallpaperconfig.h"
#include "settingsdialog.h"

#include <dfm-base/dfm_menu_defines.h>

#include <QVariantHash>
#include <QMenu>
#include <QApplication>
#include <QTimer>

using namespace ddplugin_videowallpaper;
DFMBASE_USE_NAMESPACE

AbstractMenuScene *VideoWallpaerMenuCreator::create()
{
    return new VideoWallpaperMenuScene();
}

VideoWallpaperMenuScene::VideoWallpaperMenuScene(QObject *parent)
    : AbstractMenuScene(parent)
{
    predicateName[ActionID::kVideoWallpaper] = tr("动态壁纸");
    predicateName[ActionID::kVideoWallpaperSettings] = tr("动态壁纸设置…");
}

QString VideoWallpaperMenuScene::name() const
{
    return VideoWallpaerMenuCreator::name();
}

bool VideoWallpaperMenuScene::initialize(const QVariantHash &params)
{
    turnOn = WpCfg->enable();
    isEmptyArea = params.value(MenuParamKey::kIsEmptyArea).toBool();
    onDesktop = params.value(MenuParamKey::kOnDesktop).toBool();
    return isEmptyArea && onDesktop;
}

AbstractMenuScene *VideoWallpaperMenuScene::scene(QAction *action) const
{
    if (!action)
        return nullptr;
    if (predicateAction.values().contains(action))
        return const_cast<VideoWallpaperMenuScene *>(this);
    return AbstractMenuScene::scene(action);
}

bool VideoWallpaperMenuScene::create(QMenu *parent)
{
    Q_UNUSED(parent)
    // 只创建 Action，不直接 add 进菜单；由 updateState 插入，避免重复/空指针
    auto *toggle = new QAction(predicateName.value(ActionID::kVideoWallpaper), this);
    toggle->setProperty(ActionPropertyKey::kActionID, QString(ActionID::kVideoWallpaper));
    toggle->setCheckable(true);
    toggle->setChecked(turnOn);
    predicateAction[ActionID::kVideoWallpaper] = toggle;

    auto *settings = new QAction(predicateName.value(ActionID::kVideoWallpaperSettings), this);
    settings->setProperty(ActionPropertyKey::kActionID, QString(ActionID::kVideoWallpaperSettings));
    predicateAction[ActionID::kVideoWallpaperSettings] = settings;
    return true;
}

void VideoWallpaperMenuScene::updateState(QMenu *parent)
{
    if (!parent)
        return;

    auto *toggle = predicateAction.value(ActionID::kVideoWallpaper);
    auto *settings = predicateAction.value(ActionID::kVideoWallpaperSettings);
    if (!toggle || !settings)
        return;

    toggle->setChecked(WpCfg->enable());

    auto actions = parent->actions();
    auto actionIter = std::find_if(actions.begin(), actions.end(), [](const QAction *ac) {
        return ac && ac->property(ActionPropertyKey::kActionID).toString() == QStringLiteral("wallpaper-settings");
    });

    if (actionIter != actions.end()) {
        QAction *indexAction = *actionIter;
        parent->insertAction(indexAction, settings);
        parent->insertAction(settings, toggle);
    } else {
        parent->addAction(toggle);
        parent->addAction(settings);
    }
    AbstractMenuScene::updateState(parent);
}

bool VideoWallpaperMenuScene::triggered(QAction *action)
{
    if (!action || !predicateAction.values().contains(action))
        return AbstractMenuScene::triggered(action);

    const auto actionId = action->property(ActionPropertyKey::kActionID).toString();
    if (actionId == ActionID::kVideoWallpaper) {
        emit WpCfg->changeEnableState(action->isChecked());
        if (WpCfg->enable())
            emit WpCfg->checkResource();
        return true;
    }
    if (actionId == ActionID::kVideoWallpaperSettings) {
        // 菜单回调可能不在 GUI 线程：丢回主线程，下一拍再弹窗，避免闪退
        QTimer::singleShot(0, qApp, []() {
            showSettingsDialog();
        });
        return true;
    }
    return AbstractMenuScene::triggered(action);
}
