// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include "wallpaperconfig.h"

#include <QDialog>
#include <QHash>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QListWidget;
class QListWidgetItem;
class QLabel;
class QLineEdit;
class QPushButton;
class QGroupBox;

namespace ddplugin_videowallpaper {

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    void reload();

private slots:
    void onScreenChanged(QListWidgetItem *cur, QListWidgetItem *prev);
    void onItemChanged(QListWidgetItem *item);
    void browseVideo();
    void clearVideo();
    void onVideoEdited(const QString &path);
    void applyAndClose();
    void applyOnly();

private:
    void buildUi();
    void loadFromConfig();
    void collectToConfig();
    void refreshOptionEnabled();
    void syncChecksToMap();
    QStringList detectScreens() const;

    QCheckBox *enableBox = nullptr;
    QListWidget *screenList = nullptr;
    QLineEdit *videoEdit = nullptr;
    QPushButton *browseBtn = nullptr;
    QPushButton *clearBtn = nullptr;
    QGroupBox *optionBox = nullptr;
    QComboBox *decodeBox = nullptr;
    QComboBox *qualityBox = nullptr;
    QComboBox *fpsBox = nullptr;
    QComboBox *fillBox = nullptr;
    QComboBox *smoothBox = nullptr;
    QDoubleSpinBox *speedSpin = nullptr;
    QLabel *tipLabel = nullptr;
    bool loading = false;

    QHash<QString, ScreenSetting> screenMap;
};

// 必须在 GUI 主线程调用；菜单里请用 QueuedConnection 抛过来
void showSettingsDialog();

}

#endif
