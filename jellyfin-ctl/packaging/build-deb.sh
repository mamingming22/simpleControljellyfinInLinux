#!/bin/bash
set -e
DIR=/home/matel/deb-build/jellyfin-ctl
rm -rf "$DIR"
mkdir -p "$DIR/DEBIAN"
mkdir -p "$DIR/usr/local/bin"
mkdir -p "$DIR/usr/share/applications"
mkdir -p "$DIR/usr/share/pixmaps"
mkdir -p "$DIR/etc/sudoers.d"

cp /home/matel/rpmbuild/SOURCES/jellyfin-ctl-1.0/usr/local/bin/jellyfin-ctl "$DIR/usr/local/bin/"
cp /home/matel/rpmbuild/SOURCES/jellyfin-ctl-1.0/usr/share/applications/jellyfin-ctl.desktop "$DIR/usr/share/applications/"
cp /home/matel/rpmbuild/SOURCES/jellyfin-ctl-1.0/usr/share/pixmaps/jellyfin-ctl.svg "$DIR/usr/share/pixmaps/"
cp /home/matel/rpmbuild/SOURCES/jellyfin-ctl-1.0/usr/share/pixmaps/jellyfin-ctl.png "$DIR/usr/share/pixmaps/"
cp /home/matel/rpmbuild/SOURCES/jellyfin-ctl-1.0/etc/sudoers.d/jellyfin-ctl "$DIR/etc/sudoers.d/"
chmod 755 "$DIR/usr/local/bin/jellyfin-ctl"
chmod 440 "$DIR/etc/sudoers.d/jellyfin-ctl"

cat > "$DIR/DEBIAN/control" <<EOF
Package: jellyfin-ctl
Version: 1.0-7
Section: utils
Priority: optional
Architecture: all
Maintainer: matel <matel@fedora>
Depends: python3, python3-pyside6, sudo, systemd
Description: Jellyfin 启动/停止控制面板
 一个极简的 Jellyfin 服务控制面板，提供启动/停止按钮和系统托盘。
EOF

cat > "$DIR/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e
if [ -d /etc/sudoers.d ]; then
    chmod 440 /etc/sudoers.d/jellyfin-ctl
fi
EOF
chmod 755 "$DIR/DEBIAN/postinst"

cd /home/matel/deb-build
fakeroot dpkg-deb --build jellyfin-ctl 2>/dev/null || {
    # manual build with ar/tar
    PKG=jellyfin-ctl
    VR=1.0-7
    mkdir -p /tmp/debbuild
    cd /tmp/debbuild
    rm -rf "jellyfin-ctl_${VR}_all"
    cp -a "$DIR" "jellyfin-ctl_${VR}_all"
    
    # data.tar.gz
    cd "jellyfin-ctl_${VR}_all"
    tar czf /tmp/debbuild/data.tar.gz usr/ etc/ --owner=root --group=root 2>/dev/null
    
    # control.tar.gz
    cd DEBIAN
    tar czf /tmp/debbuild/control.tar.gz . --owner=root --group=root 2>/dev/null
    cd /tmp/debbuild
    
    # debian-binary
    echo "2.0" > debian-binary
    
    # ar archive
    ar rcs "/home/matel/deb-build/jellyfin-ctl_${VR}_all.deb" debian-binary control.tar.gz data.tar.gz 2>/dev/null
    
    rm -rf /tmp/debbuild
}

echo "Done: /home/matel/deb-build/jellyfin-ctl_1.0-7_all.deb"
