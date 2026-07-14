# 开发记忆

## 项目概述

Jellyfin 系统托盘控制面板 — 用 C + GTK3 编写，提供启动/停止/重启服务和打开浏览器的 GUI。

## 技术栈

- **语言**: C11
- **GUI**: GTK3 + libayatana-appindicator（AppIndicator，Wayland 兼容）
- **构建**: Makefile + pkg-config
- **服务控制**: systemd + sudo（sudoers 免密）
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

### 按钮自动禁用

- `update_label()` 每 3 秒运行 `systemctl is-active jellyfin`
- 根据返回的 `active`/`inactive`/`failed` 切换按钮和菜单项 enable/disable

### 窗口生命周期

- `on_delete` + `gtk_widget_hide()` → 窗口关闭隐藏到托盘
- 窗口底部「退出」按钮 / 托盘菜单「退出」 → `gtk_main_quit()` 完全退出

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

- sudoers 文件 `/etc/sudoers.d/jellyfin-ctl`：
  ```
  %sudo ALL=(ALL) NOPASSWD: /usr/bin/systemctl start jellyfin, /usr/bin/systemctl stop jellyfin, /usr/bin/systemctl restart jellyfin
  ```
- Debian 用 `%sudo`，Fedora 用 `%wheel`

## 已知限制

- 仅在 KDE Plasma (Wayland) 下测试
- 使用 GtkStatusIcon 回退时，Wayland 下无托盘图标
- libayatana-appindicator C API 已废弃（推荐 GLib 版），但功能正常

## 开发历史

1. tkinter 原型
2. PySide6 重写
3. sudoers 免密
4. tkinter + pystray 回退（消除 PySide6 依赖）
5. C + GTK3 重写（GtkStatusIcon → libayatana-appindicator for Wayland）
6. install.sh 替代 DEB/RPM 打包
7. 按钮状态自动禁用
8. 窗口退出按钮 + 完全退出
