# jellyfin-ctl

Jellyfin 服务控制面板 — 系统托盘小工具，支持启动、停止、重启服务，以及打开浏览器访问。

## 快速安装（通用 Linux）

```bash
git clone https://github.com/你的用户名/jellyfin-ctl.git
cd jellyfin-ctl
chmod +x install.sh
./install.sh
```

脚本会自动检测发行版并安装依赖。

## 功能

- 系统托盘常驻，Jellyfin 图标
- 启动 / 停止 / 重启 Jellyfin 服务
- 自定义端口 + 打开浏览器
- 单实例运行，菜单栏再次点击弹出已有窗口
- 关闭窗口最小化到托盘
- 托盘右键退出时自动停止服务 + 清理临时文件
- 无需重复输入密码（sudoers 免密规则）

## 安装

### RPM（Fedora）

```bash
sudo dnf install python3-pyside6
sudo rpm -ivh jellyfin-ctl-1.0-7.fc44.noarch.rpm
```

### DEB（Debian / Ubuntu）

```bash
sudo apt install python3-pyside6
sudo dpkg -i jellyfin-ctl_1.0-7_all.deb
sudo apt install -f   # 补全依赖
```

如果 `python3-pyside6` 在您的发行版中不存在，请搜索 `python3-pyside` 或 `python3-qt6` 替代。

## 构建

```bash
# RPM
sudo dnf install rpm-build
rpmbuild -ba packaging/jellyfin-ctl.spec

# DEB
sudo apt install dpkg-dev
bash packaging/build-deb.sh
```

## 项目结构

```
jellyfin-ctl/
├── src/
│   ├── jellyfin-ctl          # 主程序
│   ├── jellyfin-ctl.desktop  # 桌面菜单入口
│   └── jellyfin-ctl.sudoers  # sudoers 免密规则
├── pixmaps/
│   ├── jellyfin-ctl.svg      # 图标（SVG）
│   └── jellyfin-ctl.png      # 图标（PNG）
├── packaging/
│   ├── jellyfin-ctl.spec     # RPM 构建配置
│   └── build-deb.sh          # DEB 构建脚本
└── README.md
```

## 依赖

- Python 3
- PySide6（Qt6 绑定）
- systemd
- sudo

## 开发参考

`MEMORY.md` 记录了技术细节和开发记忆，适合 AI 或贡献者快速接手。

## 许可

MIT
