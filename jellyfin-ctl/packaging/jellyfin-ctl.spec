Name:           jellyfin-ctl
Version:        1.0
Release:        7%{?dist}
Summary:        Jellyfin 启动/停止控制面板

License:        MIT
URL:            https://github.com/matel/jellyfin-ctl
Source0:        %{name}-%{version}.tar.gz

Requires:       python3
Requires:       python3-pyside6
Requires:       sudo
Requires:       systemd

BuildArch:      noarch

%description
一个极简的 Jellyfin 服务控制面板，提供启动/停止按钮。

%prep
%setup -q

%install
mkdir -p %{buildroot}/usr/local/bin
mkdir -p %{buildroot}/usr/share/applications
mkdir -p %{buildroot}/usr/share/pixmaps
mkdir -p %{buildroot}/etc/sudoers.d
install -Dm755 usr/local/bin/jellyfin-ctl %{buildroot}/usr/local/bin/jellyfin-ctl
install -Dm644 usr/share/applications/jellyfin-ctl.desktop %{buildroot}/usr/share/applications/jellyfin-ctl.desktop
install -Dm644 usr/share/pixmaps/jellyfin-ctl.svg %{buildroot}/usr/share/pixmaps/jellyfin-ctl.svg
install -Dm644 usr/share/pixmaps/jellyfin-ctl.png %{buildroot}/usr/share/pixmaps/jellyfin-ctl.png
install -Dm440 etc/sudoers.d/jellyfin-ctl %{buildroot}/etc/sudoers.d/jellyfin-ctl

%files
%attr(755, root, root) /usr/local/bin/jellyfin-ctl
%attr(644, root, root) /usr/share/applications/jellyfin-ctl.desktop
%attr(644, root, root) /usr/share/pixmaps/jellyfin-ctl.svg
%attr(644, root, root) /usr/share/pixmaps/jellyfin-ctl.png
%attr(440, root, root) /etc/sudoers.d/jellyfin-ctl

%changelog
* Sun Jul 12 2026 matel <matel@fedora> - 1.0-7
- sudoers 免密，无需多次输入密码
* Sun Jul 12 2026 matel <matel@fedora> - 1.0-6
- 退出时停止 Jellyfin 服务
* Sun Jul 12 2026 matel <matel@fedora> - 1.0-5
- 改用 PySide6，添加系统托盘和单实例
* Sun Jul 12 2026 matel <matel@fedora> - 1.0-4
- 添加 Jellyfin 图标和修改窗口标题
* Sun Jul 12 2026 matel <matel@fedora> - 1.0-3
- 添加端口输入和打开浏览器按钮
* Sun Jul 12 2026 matel <matel@fedora> - 1.0-2
- 修复权限和桌面文件路径
* Sun Jul 12 2026 matel <matel@fedora> - 1.0-1
- 初始版本
