#!/bin/bash
set -e

SELF="$(readlink -f "$0")"
DIR="$(dirname "$SELF")"

error() { echo -e "\033[31m错误: $*\033[0m" >&2; exit 1; }
info()  { echo -e "\033[36m=> $*\033[0m"; }

if [ "$(id -u)" -ne 0 ]; then
    exec sudo "$SELF" "$@"
fi

info "正在安装 jellyfin-ctl..."

# 检测发行版
detect_pkg() {
    if command -v dnf &>/dev/null; then
        PKG_INST="dnf install -y"
        PY_QT="python3-pyside6"
    elif command -v apt &>/dev/null; then
        PKG_INST="apt install -y"
        PY_QT="python3-pyside6"
    elif command -v pacman &>/dev/null; then
        PKG_INST="pacman -S --noconfirm"
        PY_QT="python-pyside6"
    elif command -v zypper &>/dev/null; then
        PKG_INST="zypper install -y"
        PY_QT="python3-pyside6"
    else
        error "未识别的包管理器，请手动安装依赖: python3 + pyside6 + sudo + systemd"
    fi
}

detect_pkg

# 安装依赖
info "安装依赖..."
$PKG_INST $PY_QT sudo 2>&1 | tail -3 || info "依赖可能已存在，继续..."

# 复制主程序
info "复制文件..."
install -Dm755 "$DIR/src/jellyfin-ctl" /usr/local/bin/jellyfin-ctl
install -Dm644 "$DIR/src/jellyfin-ctl.desktop" /usr/share/applications/jellyfin-ctl.desktop
install -Dm644 "$DIR/pixmaps/jellyfin-ctl.svg" /usr/share/pixmaps/jellyfin-ctl.svg
install -Dm644 "$DIR/pixmaps/jellyfin-ctl.png" /usr/share/pixmaps/jellyfin-ctl.png

# sudoers 免密（可选）
if [ -f "$DIR/src/jellyfin-ctl.sudoers" ]; then
    install -Dm440 "$DIR/src/jellyfin-ctl.sudoers" /etc/sudoers.d/jellyfin-ctl
    info "已安装 sudoers 免密规则（wheel 组用户免密控制 jellyfin）"
fi

# 更新桌面数据库
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
echo ""
echo "如需卸载:"
echo "  sudo rm -f /usr/local/bin/jellyfin-ctl"
echo "  sudo rm -f /usr/share/applications/jellyfin-ctl.desktop"
echo "  sudo rm -f /usr/share/pixmaps/jellyfin-ctl.{svg,png}"
echo "  sudo rm -f /etc/sudoers.d/jellyfin-ctl"
