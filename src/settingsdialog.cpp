// SPDX-License-Identifier: GPL-3.0-or-later
#include "settingsdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QAbstractItemView>
#include <QPushButton>
#include <QGuiApplication>
#include <QScreen>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QDir>
#include <QApplication>
#include <QPointer>
#include <QDebug>

namespace ddplugin_videowallpaper {

static QPointer<SettingsDialog> g_dialog;

void showSettingsDialog()
{
    // 独立顶层窗，不挂到桌面根窗口上，避免跟着桌面一起被裁/崩
    if (!g_dialog) {
        g_dialog = new SettingsDialog(nullptr);
        g_dialog->setAttribute(Qt::WA_DeleteOnClose, true);
        QObject::connect(g_dialog, &QObject::destroyed, []() {
            g_dialog = nullptr;
        });
    }
    g_dialog->reload();
    g_dialog->show();
    g_dialog->raise();
    g_dialog->activateWindow();
}

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("动态壁纸设置"));
    // 独立普通对话框，不要 StayOnTop（在 Deepin 桌面进程里容易异常）
    setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint | Qt::WindowTitleHint);
    setWindowModality(Qt::NonModal);
    setMinimumSize(620, 520);
    buildUi();
}

QStringList SettingsDialog::detectScreens() const
{
    QStringList names;
    // 只用 QScreen，避免在弹窗路径里再去推桌面 slot（更稳）
    for (QScreen *s : QGuiApplication::screens()) {
        if (!s)
            continue;
        const QString n = s->name();
        if (!n.isEmpty() && !names.contains(n))
            names << n;
    }
    // 合并已保存过的屏名（热插拔后仍能看到配置）
    for (const QString &n : WpCfg->screenSettings().keys()) {
        if (!names.contains(n))
            names << n;
    }
    return names;
}

void SettingsDialog::buildUi()
{
    auto *root = new QVBoxLayout(this);

    enableBox = new QCheckBox(tr("启用动态壁纸"), this);
    root->addWidget(enableBox);

    // 独立开关：控制桌面左上角 fps 叠层，点应用立即生效
    showFpsBox = new QCheckBox(tr("显示帧率叠层（桌面左上角：实际 | 设置目标 | 分辨率）"), this);
    showFpsBox->setToolTip(tr("勾选后在壁纸左上角显示实时帧率；取消则隐藏。与「帧率」播放选项无关。"));
    root->addWidget(showFpsBox);

    auto *screenBox = new QGroupBox(tr("屏幕（勾选要显示的屏，并选视频）"), this);
    auto *screenLay = new QVBoxLayout(screenBox);
    screenList = new QListWidget(screenBox);
    screenList->setSelectionMode(QAbstractItemView::SingleSelection);
    screenLay->addWidget(screenList);

    auto *videoRow = new QHBoxLayout;
    videoEdit = new QLineEdit(screenBox);
    videoEdit->setPlaceholderText(tr("该屏幕的视频文件路径…"));
    browseBtn = new QPushButton(tr("浏览…"), screenBox);
    clearBtn = new QPushButton(tr("清除"), screenBox);
    videoRow->addWidget(videoEdit, 1);
    videoRow->addWidget(browseBtn);
    videoRow->addWidget(clearBtn);
    screenLay->addLayout(videoRow);
    root->addWidget(screenBox, 1);

    optionBox = new QGroupBox(tr("播放选项"), this);
    auto *form = new QFormLayout(optionBox);

    // 可切换：CPU 软解 ↔ GPU 硬解（点「应用」后立刻重建解码器）
    decodeBox = new QComboBox(optionBox);
    decodeBox->addItem(tr("CPU 软解（直接解码贴桌面，简单稳）"), int(DecodeMode::Software));
    decodeBox->addItem(tr("NVIDIA 硬解 CUDA（GPU 解，仍要回传贴屏）"), int(DecodeMode::Cuda));
    decodeBox->addItem(tr("核显硬解 VAAPI"), int(DecodeMode::Vaapi));
    decodeBox->addItem(tr("自动（先 CUDA，再 VAAPI，最后软解）"), int(DecodeMode::Auto));
    decodeBox->setToolTip(tr(
        "可随时切换。X11 嵌桌面时硬解往往不比软解更省总 CPU/Xorg；"
        "选软解更接近早期「纯 CPU 显示」。点应用后立即生效。"));
    form->addRow(tr("解码切换"), decodeBox);

    decodeTipLabel = new QLabel(optionBox);
    decodeTipLabel->setWordWrap(true);
    decodeTipLabel->setStyleSheet(QStringLiteral("color:#2a6; font-size:12px;"));
    form->addRow(QString(), decodeTipLabel);
    connect(decodeBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { refreshDecodeTip(); });

    qualityBox = new QComboBox(optionBox);
    // 0/-1 实际都会按「最大屏物理宽」出图（壁纸超过屏宽无意义）
    qualityBox->addItem(tr("按最大屏分辨率（推荐）"), 0);
    qualityBox->addItem(tr("尽量清晰（不超过最大屏）"), -1);
    qualityBox->addItem(tr("最高 2560px"), 2560);
    qualityBox->addItem(tr("最高 1920px"), 1920);
    qualityBox->addItem(tr("最高 1280px"), 1280);
    form->addRow(tr("清晰度"), qualityBox);

    fpsBox = new QComboBox(optionBox);
    // 0 = 跟片源；选固定值 = 目标输出帧率（不会超过片源本身）
    fpsBox->addItem(tr("原始帧率（跟视频）"), 0);
    fpsBox->addItem(tr("目标 60 fps"), 60);
    fpsBox->addItem(tr("目标 30 fps"), 30);
    fpsBox->addItem(tr("目标 24 fps"), 24);
    fpsBox->addItem(tr("目标 144 fps（需片源≥144）"), 144);
    fpsBox->addItem(tr("目标 120 fps（需片源≥120）"), 120);
    fpsBox->addItem(tr("目标 90 fps"), 90);
    fpsBox->addItem(tr("目标 165 fps"), 165);
    fpsBox->addItem(tr("目标 240 fps"), 240);
    form->addRow(tr("帧率目标"), fpsBox);

    fillBox = new QComboBox(optionBox);
    fillBox->addItem(tr("铺满（等比裁切，无黑边）"), int(FillMode::Fill));
    fillBox->addItem(tr("自适应（完整显示，可能黑边）"), int(FillMode::Fit));
    fillBox->addItem(tr("拉伸（拉满屏，可能变形）"), int(FillMode::Stretch));
    fillBox->addItem(tr("居中（原始大小，不缩放）"), int(FillMode::Center));
    fillBox->addItem(tr("平铺（重复铺满）"), int(FillMode::Tile));
    form->addRow(tr("铺屏方式"), fillBox);

    smoothBox = new QComboBox(optionBox);
    smoothBox->addItem(tr("快速（锯齿多，最省）"), int(SmoothLevel::Fast));
    smoothBox->addItem(tr("标准"), int(SmoothLevel::Normal));
    smoothBox->addItem(tr("高（推荐）"), int(SmoothLevel::High));
    smoothBox->addItem(tr("最高（Lanczos，更吃资源）"), int(SmoothLevel::Highest));
    form->addRow(tr("平滑等级"), smoothBox);

    speedSpin = new QDoubleSpinBox(optionBox);
    speedSpin->setRange(0.01, 4.0);
    speedSpin->setSingleStep(0.01);
    speedSpin->setDecimals(2);
    speedSpin->setSuffix(tr(" x"));
    form->addRow(tr("播放速度"), speedSpin);

    tipLabel = new QLabel(optionBox);
    tipLabel->setWordWrap(true);
    tipLabel->setStyleSheet(QStringLiteral("color:#666;"));
    tipLabel->setText(tr(
        "【解码切换】CPU 软解 / CUDA / VAAPI / 自动，点应用立即切换，无需重装。"
        "【帧率】原始=跟片源；选 30 可明显降占用。"
        "左上角叠层只显示统计，不改播放。"));
    form->addRow(tipLabel);
    root->addWidget(optionBox);

    auto *buttons = new QDialogButtonBox(this);
    auto *applyBtn = buttons->addButton(tr("应用"), QDialogButtonBox::ApplyRole);
    auto *okBtn = buttons->addButton(tr("确定"), QDialogButtonBox::AcceptRole);
    auto *cancelBtn = buttons->addButton(tr("取消"), QDialogButtonBox::RejectRole);
    root->addWidget(buttons);

    connect(screenList, &QListWidget::currentItemChanged, this, &SettingsDialog::onScreenChanged);
    connect(screenList, &QListWidget::itemChanged, this, &SettingsDialog::onItemChanged);
    connect(browseBtn, &QPushButton::clicked, this, &SettingsDialog::browseVideo);
    connect(clearBtn, &QPushButton::clicked, this, &SettingsDialog::clearVideo);
    connect(videoEdit, &QLineEdit::textEdited, this, &SettingsDialog::onVideoEdited);
    connect(applyBtn, &QPushButton::clicked, this, &SettingsDialog::applyOnly);
    connect(okBtn, &QPushButton::clicked, this, &SettingsDialog::applyAndClose);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(enableBox, &QCheckBox::toggled, this, [this](bool) { refreshOptionEnabled(); });
}

void SettingsDialog::reload()
{
    loadFromConfig();
}

void SettingsDialog::loadFromConfig()
{
    loading = true;
    enableBox->setChecked(WpCfg->enable());
    showFpsBox->setChecked(WpCfg->showFps());
    screenMap = WpCfg->screenSettings();

    screenList->clear();
    const QStringList screens = detectScreens();
    for (const QString &name : screens) {
        if (!screenMap.contains(name)) {
            ScreenSetting ss;
            ss.enabled = true; // 默认各屏都可选中
            screenMap.insert(name, ss);
        }
        auto *item = new QListWidgetItem(name, screenList);
        item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        item->setCheckState(screenMap.value(name).enabled ? Qt::Checked : Qt::Unchecked);
        item->setData(Qt::UserRole, name);
    }
    if (screenList->count() > 0)
        screenList->setCurrentRow(0);

    int modeIdx = decodeBox->findData(int(WpCfg->decodeMode()));
    // 缺省/未知：CPU 软解（第一项）
    decodeBox->setCurrentIndex(modeIdx < 0 ? 0 : modeIdx);
    refreshDecodeTip();

    int qIdx = qualityBox->findData(WpCfg->maxWidth());
    qualityBox->setCurrentIndex(qIdx < 0 ? 0 : qIdx);

    const int fpsKey = qRound(WpCfg->fps());
    int fIdx = fpsBox->findData(fpsKey);
    if (fIdx < 0)
        fIdx = fpsBox->findData(0);
    fpsBox->setCurrentIndex(fIdx < 0 ? 0 : fIdx);

    speedSpin->setValue(WpCfg->speed());

    int sIdx = smoothBox->findData(int(WpCfg->smoothLevel()));
    // 默认：快速（关掉平滑）
    smoothBox->setCurrentIndex(sIdx < 0 ? 0 : sIdx);

    int fillIdx = fillBox->findData(int(WpCfg->fillMode()));
    fillBox->setCurrentIndex(fillIdx < 0 ? 0 : fillIdx);

    loading = false;

    onScreenChanged(screenList->currentItem(), nullptr);
    refreshOptionEnabled();
}

void SettingsDialog::onScreenChanged(QListWidgetItem *cur, QListWidgetItem *)
{
    if (!cur) {
        videoEdit->clear();
        videoEdit->setEnabled(false);
        browseBtn->setEnabled(false);
        clearBtn->setEnabled(false);
        return;
    }
    const QString name = cur->data(Qt::UserRole).toString();
    videoEdit->setEnabled(true);
    browseBtn->setEnabled(true);
    clearBtn->setEnabled(true);
    videoEdit->setText(screenMap.value(name).video);
    refreshOptionEnabled();
}

void SettingsDialog::onItemChanged(QListWidgetItem *)
{
    if (loading)
        return;
    syncChecksToMap();
    refreshOptionEnabled();
}

void SettingsDialog::syncChecksToMap()
{
    for (int i = 0; i < screenList->count(); ++i) {
        auto *item = screenList->item(i);
        if (!item)
            continue;
        const QString name = item->data(Qt::UserRole).toString();
        ScreenSetting ss = screenMap.value(name);
        ss.enabled = (item->checkState() == Qt::Checked);
        screenMap.insert(name, ss);
    }
}

void SettingsDialog::browseVideo()
{
    auto *item = screenList->currentItem();
    if (!item)
        return;
    const QString path = QFileDialog::getOpenFileName(
                this, tr("选择视频"),
                QDir::homePath() + QStringLiteral("/Videos"),
                tr("视频 (*.mp4 *.mkv *.webm *.avi *.mov *.m4v);;所有文件 (*)"));
    if (path.isEmpty())
        return;
    videoEdit->setText(path);
    onVideoEdited(path);
    item->setCheckState(Qt::Checked);
    syncChecksToMap();
    refreshOptionEnabled();
}

void SettingsDialog::clearVideo()
{
    videoEdit->clear();
    onVideoEdited(QString());
}

void SettingsDialog::onVideoEdited(const QString &path)
{
    auto *item = screenList->currentItem();
    if (!item)
        return;
    const QString name = item->data(Qt::UserRole).toString();
    ScreenSetting ss = screenMap.value(name);
    ss.video = path.trimmed();
    screenMap.insert(name, ss);
    refreshOptionEnabled();
}

void SettingsDialog::refreshOptionEnabled()
{
    optionBox->setEnabled(true);
    Q_UNUSED(enableBox)
}

void SettingsDialog::refreshDecodeTip()
{
    if (!decodeTipLabel || !decodeBox)
        return;
    const DecodeMode m = DecodeMode(decodeBox->currentData().toInt());
    switch (m) {
    case DecodeMode::Software:
        decodeTipLabel->setText(tr("当前：CPU 软解 → 直接出 RGB 贴桌面（无独立窗）。总占用与硬解在 X11 上往往接近。"));
        break;
    case DecodeMode::Cuda:
        decodeTipLabel->setText(tr("当前：NVIDIA CUDA 硬解 → 仍要回传内存再贴桌面。可切换回「CPU 软解」对比。"));
        break;
    case DecodeMode::Vaapi:
        decodeTipLabel->setText(tr("当前：VAAPI 核显硬解 → 同样要回传+贴屏。失败时请改 CPU 软解或自动。"));
        break;
    case DecodeMode::Auto:
    default:
        decodeTipLabel->setText(tr("当前：自动尝试 CUDA → VAAPI → 软解。需要固定路径请手动选 CPU 或 CUDA。"));
        break;
    }
}

void SettingsDialog::collectToConfig()
{
    syncChecksToMap();
    const bool oldEnable = WpCfg->enable();
    const bool newEnable = enableBox->isChecked();

    WpCfg->setFps(fpsBox->currentData().toDouble());
    WpCfg->setSpeed(speedSpin->value());
    WpCfg->setMaxWidth(qualityBox->currentData().toInt());
    WpCfg->setDecodeMode(DecodeMode(decodeBox->currentData().toInt()));
    WpCfg->setSmoothLevel(SmoothLevel(smoothBox->currentData().toInt()));
    WpCfg->setFillMode(FillMode(fillBox->currentData().toInt()));
    WpCfg->setShowFps(showFpsBox->isChecked());
    WpCfg->setScreenSettings(screenMap);
    WpCfg->setEnable(newEnable);
    WpCfg->save();

    if (oldEnable != newEnable)
        emit WpCfg->changeEnableState(newEnable);
}

void SettingsDialog::applyOnly()
{
    try {
        collectToConfig();
        if (WpCfg->enable())
            emit WpCfg->checkResource();
    } catch (...) {
        qWarning() << "[videowallpaper] apply settings failed";
    }
}

void SettingsDialog::applyAndClose()
{
    applyOnly();
    accept();
}

} // namespace ddplugin_videowallpaper
