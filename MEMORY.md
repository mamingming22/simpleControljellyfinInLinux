# 开发记忆

## 项目概述

- **版本**: 1.1
- Jellyfin 系统托盘控制面板 — 用 C + GTK3 编写，提供启动/停止/重启服务和打开浏览器的 GUI。

## 技术栈

- **语言**: C11
- **GUI**: GTK3 + libayatana-appindicator（AppIndicator，Wayland 兼容）
- **国际化**: `_(zh, en)` 宏 + 窗口内语言切换按钮（默认中文）
- **构建**: Makefile + pkg-config
- **服务控制**: systemd / Flatpak / Snap 三种格式自动检测
- **单实例**: flock 文件锁 + SIGUSR1 信号

## 架构

```
jellyfin-ctl/
├── install.sh           # 一键编译安装/卸载（-u / --uninstall）
└── src/
    ├── jellyfin-ctl.c   # 主程序
    ├── Makefile          # 构建
    ├── jellyfin-ctl.desktop
    ├── jellyfin-ctl.sudoers
    └── jellyfin-ctl.{svg,png}  # 图标
```

## 关键实现细节

### 单实例机制

- PID 文件锁 `/tmp/jellyfin-ctl.lock`
- `flock(LOCK_EX | LOCK_NB)` 原子性检测
- 第二个实例读取锁文件中的 PID，发送 `SIGUSR1` 让第一个实例弹窗
- `g_unix_signal_add(SIGUSR1, ...)` 在 GTK 主循环中处理信号

### 托盘机制

- 使用 `libayatana-appindicator`（StatusNotifierItem 协议）
- Wayland（KDE Plasma）下正常显示
- 菜单持久化（一次性创建，不重复 new/free）
- `gtk_widget_set_sensitive` 根据状态禁用菜单项

### 多格式自动检测

- 启动时自动检测三种格式：
  - **systemd**: `which jellyfin` 或 `systemctl list-unit-files jellyfin.service`
  - **Flatpak**: `flatpak list --app --columns=application` 中查找含 `jellyfin` 的条目
  - **Snap**: `snap list` 中查找含 `jellyfin` 的条目
- 检测到多个格式时，窗口内显示下拉框（`GtkComboBoxText`）供用户选择
- 选择切换后实时更新状态显示和控制行为
- 检测结果为空时，显示「未检测到 Jellyfin」+「刷新」按钮，可手动重新检测

### 按钮自动禁用

- `update_label()` 每 5 秒检测服务状态（根据当前模式使用不同命令）
- 支持三种格式：
  - **systemd**: `systemctl is-active jellyfin`
  - **Flatpak**: `flatpak ps --columns=application` 查找 ID
  - **Snap**: `snap services <name>` 检查状态列
- 根据检测到的 `active`/`inactive` 切换按钮和菜单项 enable/disable

### 窗口生命周期

- `on_delete` + `gtk_widget_hide()` → 窗口关闭隐藏到托盘
- 窗口底部「退出」按钮 / 托盘菜单「退出」 → `gtk_main_quit()` 完全退出

### 中/英文切换

- `is_zh` 全局布尔变量，默认 `TRUE`（中文）
- `_(zh, en)` 宏根据 `is_zh` 选择字符串
- 窗口底部 `[English]`/`[中文]` 按钮，点击后切换 `is_zh`，更新所有按钮/标签/菜单文本，重建 `top_section`
- `on_lang_changed` 更新所有已存储的 widget 引用 + 调用 `do_refresh(FALSE)` 重建顶部区域

## 编译依赖

| 系统 | 包名 |
|------|------|
| Debian/Ubuntu | gcc, pkgconf, libgtk-3-dev, libayatana-appindicator3-dev, make |
| Fedora | gcc, pkgconfig, gtk3-devel, libayatana-appindicator-gtk3-devel, make |
| Arch | gcc, pkgconf, gtk3, libayatana-appindicator, make |
| openSUSE | gcc, pkgconf, gtk3-devel, libayatana-appindicator3-devel, make |

## 运行时依赖

libgtk-3-0t64, libayatana-appindicator3-1, sudo, systemd

## 权限方案

- **systemd**: sudoers 文件 `/etc/sudoers.d/jellyfin-ctl`（`sudo systemctl start/stop/restart jellyfin`）
  ```
  %sudo ALL=(ALL) NOPASSWD: /usr/bin/systemctl start jellyfin, /usr/bin/systemctl stop jellyfin, /usr/bin/systemctl restart jellyfin
  ```
- **Flatpak**: 用户级运行，无需 sudo
- **Snap**: 需要 sudo（`sudo snap start/stop/restart <name>`）
- Debian 用 `%sudo`，Fedora 用 `%wheel`

## 已知限制

- 已在 KDE Plasma (Wayland) 和 Pop!_OS (COSMIC, X11) 下测试
- 使用 GtkStatusIcon 回退时，Wayland 下无托盘图标
- libayatana-appindicator C API 已废弃（推荐 GLib 版），但功能正常
- Snap 包 `itrue-jellyfin` 为社区维护，非官方
- Flatpak 版 `org.jellyfin.server` 的停止通过 `flatpak kill` 实现，可能不如 systemd 干净

## 开发历史

1. tkinter 原型
2. PySide6 重写
3. sudoers 免密
4. tkinter + pystray 回退（消除 PySide6 依赖）
5. C + GTK3 重写（GtkStatusIcon → libayatana-appindicator for Wayland）
6. install.sh 替代 DEB/RPM 打包
7. 按钮状态自动禁用
8. 窗口退出按钮 + 完全退出
9. 多格式检测（systemd + Flatpak + Snap）+ 下拉选择器
10. 未检测到时显示「刷新」按钮，支持手动重新检测
11. install.sh: 跳过已安装的依赖、安装时终止旧进程、版本号
12. 中/英双语切换 — 窗口内按钮手动切换语言，即时更新所有 UI 文本
13. v1.1 — 中/英双语切换正式版，修复未检测状态 markup 翻译遗漏，在 Pop!_OS (COSMIC) 下完成测试验证
