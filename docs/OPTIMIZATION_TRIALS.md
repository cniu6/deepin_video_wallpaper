# 优化试验总表（含 GPU 显示路径）

## 环境
- X11 + kwin_x11，双屏 2560+1920
- 片源示例 3840x2160 → 缩到屏宽

## 路径演进
| 版本 | 路径 | 正播约（单核可叠加） |
|------|------|----------------------|
| 1.4.2 | QImage + drawImage + sws→RGB | dde~100% Xorg~32% |
| 1.5.0 | GL + 无缩放才 NV12（4K 常退 RGB） | Xorg 略降 |
| **1.5.1** | **sws→NV12 + GL 双平面着色** | **dde~64% Xorg~24%** |

## 仍非零拷贝
硬解帧仍 `transfer` 到系统内存；无 dde DMA-BUF 壁纸 API。
GPU 侧收益：跳过 BGRA sws 全像素 + 软件 QPainter 贴图，改为 NV12 上传 + shader。
