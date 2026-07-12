# 开发记忆

## 项目概述

Jellyfin 系统托盘控制面板，提供启动/停止/重启服务和打开浏览器的 GUI 工具。

## 技术栈

- **语言**: Python 3
- **GUI**: PySide6 (Qt6)
- **服务控制**: systemd + sudo（sudoers 免密）
- **单实例**: QLocalServer / QLocalSocket (QtNetwork)

## 架构

```
src/jellyfin-ctl  — 主程序（Python + PySide6）
```

### 单实例机制

- 首次启动: 创建 `QLocalServer`，监听 `jellyfin-ctl-socket`
- 再次启动: `QLocalSocket` 尝试连接，若成功则发送信号弹出已有窗口后退出
- 退出时: `aboutToQuit` 信号关闭 server

### 图标设置

- 窗口图标: `setWindowIcon(QIcon(SVG))`
- 托盘图标: `QSystemTrayIcon(QIcon(SVG))`
- SVG 从 jellyfin-web 包中的 logo 裁剪获得

### 权限方案

- `sudo` + sudoers 规则，允许 `wheel` 组免密执行 `systemctl start/stop/restart jellyfin`
- 不再使用 `pkexec`（Wayland 兼容性差）

### 托盘双击检测

- KDE/Wayland 不支持标准 `DoubleClick` 事件
- 手动判断: 记录上次点击时间，400ms 内第二次点击视为双击

## 关键依赖

| 发行版 | PySide6 包名 |
|--------|-------------|
| Fedora | python3-pyside6 |
| Debian/Ubuntu | python3-pyside6（旧版: python3-pyside2） |
| Arch | python-pyside6 |
| openSUSE | python3-pyside6 |

## 打包方式

- **RPM**: `rpmbuild -ba packaging/jellyfin-ctl.spec`
- **DEB**: `bash packaging/build-deb.sh`（手动 ar+tar 构建，无需 dpkg）

## 已知限制

- 仅在 KDE Plasma (Wayland) 下测试
- 托盘功能在其他桌面环境（GNOME/GTK）可能不正常（GNOME 40+ 不兼容）

## 开发历史

1. tkinter 原型 → 窗口, 启动/停止按钮, 端口输入
2. PySide6 重写 → 系统托盘 + 单实例
3. sudoers 免密 → pkexec 替换
4. QLocalServer 单例 → 信号方案废弃
