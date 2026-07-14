#!/bin/bash
set -e
SELF="$(readlink -f "$0")"
DIR="$(dirname "$SELF")"

error() { echo -e "\033[31m错误: $*\033[0m" >&2; exit 1; }
info()  { echo -e "\033[36m=> $*\033[0m"; }

if [ "$(id -u)" -ne 0 ]; then
	exec sudo "$SELF" "$@"
fi

uninstall() {
	info "卸载 jellyfin-ctl..."
	rm -f /usr/local/bin/jellyfin-ctl
	rm -f /usr/share/applications/jellyfin-ctl.desktop
	rm -f /usr/share/pixmaps/jellyfin-ctl.svg /usr/share/pixmaps/jellyfin-ctl.png
	rm -f /etc/sudoers.d/jellyfin-ctl
	rm -f /tmp/jellyfin-ctl.lock
	info "卸载完成"
	exit 0
}

if [ "${1:-}" = "-u" ] || [ "${1:-}" = "--uninstall" ]; then
	uninstall
fi

info "正在编译安装 jellyfin-ctl..."

if command -v dnf &>/dev/null; then
	info "安装构建依赖 (dnf)..."
	dnf install -y gcc pkgconfig gtk3-devel make
elif command -v apt &>/dev/null; then
	info "安装构建依赖 (apt)..."
	apt install -y gcc pkgconf libgtk-3-dev libayatana-appindicator3-dev make
elif command -v pacman &>/dev/null; then
	info "安装构建依赖 (pacman)..."
	pacman -S --noconfirm gcc pkgconf gtk3 make
elif command -v zypper &>/dev/null; then
	info "安装构建依赖 (zypper)..."
	zypper install -y gcc pkgconf gtk3-devel make
else
	error "未识别的包管理器，请手动安装: gcc + pkgconf + gtk3-dev + make"
fi

cd "$DIR/src"
make clean 2>/dev/null || true
make

info "安装文件..."
install -Dm755 jellyfin-ctl /usr/local/bin/jellyfin-ctl
install -Dm644 jellyfin-ctl.desktop /usr/share/applications/jellyfin-ctl.desktop
install -Dm644 jellyfin-ctl.svg /usr/share/pixmaps/jellyfin-ctl.svg
install -Dm644 jellyfin-ctl.png /usr/share/pixmaps/jellyfin-ctl.png

if [ -f jellyfin-ctl.sudoers ]; then
	install -Dm440 jellyfin-ctl.sudoers /etc/sudoers.d/jellyfin-ctl
	info "已安装 sudoers 免密规则"
	info "用户需要属于 sudo 组（Debian）或 wheel 组（Fedora）才能免密控制 jellyfin"
fi

if command -v update-desktop-database &>/dev/null; then
	update-desktop-database 2>/dev/null || true
fi

echo ""
echo "========================"
echo "  安装完成！"
echo "========================"
echo ""
echo "启动命令: jellyfin-ctl"
echo "或在应用菜单中点击「Jellyfin 控制」"
