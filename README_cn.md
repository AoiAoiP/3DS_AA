# 3DS 抗锯齿插件

[![en](https://img.shields.io/badge/lang-English-blue.svg)](README.md)

一款 **Luma3DS 运行时插件**，通过后期处理 FXAA（快速近似抗锯齿）对新大三上屏进行实时抗锯齿处理，有效降低 400×240 低分辨率渲染产生的几何边缘锯齿。

## 功能特性

- **实时 FXAA** — 基于亮度通道的边缘检测 + 方向性混合
- **定点数运算** — 无需 FPU，专为 ARM11 优化
- **可配置画质** — 4 级预设（0=极速 → 3=最佳）
- **双缓冲机制** — 无画面撕裂，输入延迟 ≤ 1 帧
- **热键控制** — `L + ↑` 开关抗锯齿
- **性能叠加层** — `L + ↓` 显示帧处理时间统计
- **自动节流** — 帧处理时间超过 4ms 预算时自动暂停 AA
- **强制模式** — `L + →` 在低频硬件上强制启用

## 性能指标

| 指标 | 目标 | 状态 |
|--------|--------|--------|
| 帧处理时间 | ≤ 2.5ms（平均） | 🚧 等待实机测试 |
| 帧率影响 | ≤ 10% | 🚧 等待实机测试 |
| 内存开销 | ≤ 2MB 系统内存 | ✅（约 375KB） |
| 输入延迟 | ≤ 1 帧 | ✅（双缓冲方案） |
| 编译 | 零错误通过 | ✅ |

## 运行要求

### 硬件
- **New 3DS XL**（推荐：804MHz CPU + 2MB L2 缓存）
- Old 3DS 亦可运行，但低于 800MHz 时会自动禁用 AA

### 3DS 端软件
- **Luma3DS v13.1+**（需支持插件加载器）

### 构建环境
- **Docker Desktop**（Windows / macOS / Linux 均支持）
- **devkitPro Docker 镜像**：`devkitpro/devkitarm`

## 编译

```bash
# 清理构建
MSYS_NO_PATHCONV=1 docker run --rm -v "$(pwd)":/app -w /app devkitpro/devkitarm make clean

# 编译
MSYS_NO_PATHCONV=1 docker run --rm -v "$(pwd)":/app -w /app devkitpro/devkitarm make
```

> **Git Bash 用户注意：** 需要加 `MSYS_NO_PATHCONV=1` 前缀，防止 Git Bash 错误转换 Docker 卷路径。在 Linux/macOS 或 PowerShell 环境下可省略。

编译产物：
- `3ds_aa.3dsx` — Homebrew Launcher 格式（开发测试用）
- `3ds_aa.elf` — ELF 二进制（调试用）

## 部署

### 作为 Luma3DS 插件
```bash
# 重命名并复制到 SD 卡（将 <title_id> 替换为游戏的 Title ID）：
cp 3ds_aa.3dsx /luma/plugins/<title_id>/code.ips
```
插件将在游戏启动时自动加载。

### 开发 / 测试
```bash
# 复制到 SD 卡，通过 Homebrew Launcher 运行：
cp 3ds_aa.3dsx /3ds/

# 或通过 3dslink 无线部署（3DS 需在 hbmenu 中按 Y 进入网络接收模式）：
3dslink 3ds_aa.3dsx -a <3DS的IP地址>
```

## 项目结构

```
3DS_AA/
├── source/
│   ├── main.c           # 插件入口、帧钩子、热键处理
│   ├── framebuffer.c    # 屏幕捕获与 RGB565 像素操作
│   ├── edge_detect.c    # 基于亮度的 Sobel 边缘检测
│   ├── anti_aliasing.c  # FXAA 核心实现（定点数）
│   └── timing.c         # PMU 周期计数器与 CPU 频率检测
├── include/
│   ├── aa_plugin.h      # 主配置头文件：常量、类型、宏
│   ├── framebuffer.h    # 帧缓冲 API
│   ├── edge_detect.h    # 边缘检测 API
│   ├── anti_aliasing.h  # FXAA API
│   └── timing.h         # 性能计时 API
├── Makefile             # devkitPro 构建系统（基于 Docker）
├── README.md            # 英文说明文档
├── README_cn.md         # 中文说明文档（本文件）
├── Roadmap.md           # 完整开发计划
└── .gitignore
```

## 热键

| 按键组合 | 功能 |
|-------|--------|
| `L + ↑` | 开关抗锯齿 |
| `L + ↓` | 开关性能叠加层 |
| `L + →` | 强制启用 AA（绕过频率检测） |

## 工作原理

1. **帧捕获** — 每帧通过 `gfxGetFramebuffer()` 将上屏 RGB565 帧缓冲（400×240）拷贝到后缓冲区。

2. **亮度转换** — 使用预计算的 64K 条目查找表（ITU-R BT.601 系数），将 RGB565 像素转换为 8 位亮度值。

3. **边缘检测** — 3×3 Sobel 算子计算每个像素的梯度幅值和方向。超过对比度阈值的像素被分类为水平、垂直或对角边缘。

4. **FXAA 混合** — 对每个边缘像素：
   - 从十字邻域计算局部对比度范围
   - 根据边缘方向选择要混合的像素对
   - 混合因子（0–255）控制抗锯齿强度
   - 子像素 AA（画质预设 3）额外平滑细线特征

5. **缓冲交换** — 处理完毕的后缓冲区在 VBlank 期间写回前缓冲区，防止画面撕裂。

## 算法参考

- FXAA 3.11 — Timothy Lottes（NVIDIA, 2009）
- ITU-R BT.601 亮度系数

## 开发状态

🚧 **Alpha 阶段** — 编译已通过，等待实机测试。

- [x] 阶段 0：环境验证（Docker 构建零错误通过）
- [ ] 阶段 1：帧缓冲捕获与颜色通道操控（需要 3DS 实机验证）
- [ ] 阶段 2：FXAA 算法移植与优化（代码完成，待性能分析）
- [ ] 阶段 3：双缓冲与性能调优（代码完成，待性能分析）
- [ ] 阶段 4：热键控制与兼容性测试（代码完成，待实机测试）

## 许可证

MIT
