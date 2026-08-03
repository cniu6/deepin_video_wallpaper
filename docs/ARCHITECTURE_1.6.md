# 1.6.x 嵌入桌面 + CPU 显示

## 用户结论（正确）
在 X11 dde edge 插件内：
GPU 硬解 → 系统内存 transfer → sws → QWidget 贴屏 → X damage
总 CPU/Xorg 与「直接软解再贴屏」接近，硬解不换显示架构就省不了 Xorg。

## 1.6.1 默认
- decodeMode: **software**（设置可改回 cuda）
- 呈现: 嵌入 QWidget + 共享 QPixmap + drawPixmap
- 禁止 QOpenGLWidget 独立窗
