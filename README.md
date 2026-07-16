# jellyfin-ctl

Jellyfin 服务控制面板 — 系统托盘小工具，支持启动、停止、重启服务，以及打开浏览器访问。

自动检测 systemd / Flatpak / Snap 三种安装格式，多格式共存时可通过下拉框切换。

支持中/英文切换，窗口底部按钮一键切换语言，所有 UI 文本即时刷新。

## 安装

```bash
cd jellyfin-ctl
./install.sh
```

自动安装编译依赖（gcc、GTK3、libayatana-appindicator）、编译并安装。

## 卸载

```bash
./install.sh --uninstall
```

## 使用

```bash
jellyfin-ctl
```

或在应用菜单中点击「Jellyfin 控制」。

- **托盘图标** — 左键/右键弹出菜单
- **启动/停止/重启** — 根据服务状态自动禁用
- **关闭窗口** — 隐藏到托盘，不退出
- **退出** — 窗口底部按钮或托盘菜单，完全退出
- **端口** — 自定义后点击「打开浏览器」
- **格式选择** — 检测到多格式时显示下拉框，可随时切换
- **刷新** — 未检测到服务时显示「刷新」按钮，安装后可手动重新检测
- **语言** — 窗口底部 `English`/`中文` 按钮，一键切换中/英文

## 支持的格式

| 格式 | 检测方式 | 控制命令 |
|------|----------|----------|
| systemd | `which jellyfin` / `systemctl list-unit-files` | `sudo systemctl start/stop/restart` |
| Flatpak | `flatpak list --app` | `flatpak run / kill` |
| Snap | `snap list` | `sudo snap start/stop/restart` |

## 依赖

| 运行时 | 构建时 |
|--------|--------|
| libgtk-3-0t64, libayatana-appindicator3-1 | gcc, pkgconf, libgtk-3-dev, libayatana-appindicator3-dev, make |

## 权限

- **systemd**: 需要免密 sudo（install.sh 自动安装 sudoers 规则）
- **Flatpak**: 用户级运行，无需额外权限
- **Snap**: 需要 sudo

Debian/Ubuntu 用户需属于 `sudo` 组，Fedora/RHEL 需属于 `wheel` 组。

## 项目结构

```
jellyfin-ctl/
├── install.sh                   # 一键编译安装/卸载
└── src/
    ├── jellyfin-ctl.c           # 主程序（C + GTK3 + AppIndicator）
    ├── Makefile                 # 构建文件
    ├── jellyfin-ctl.desktop     # 桌面菜单入口
    ├── jellyfin-ctl.sudoers     # sudo 免密规则
    ├── jellyfin-ctl.svg         # 图标
    └── jellyfin-ctl.png         # 图标
```

## 许可

MIT
