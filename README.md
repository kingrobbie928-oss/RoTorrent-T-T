# RoTorrent

A Windows BitTorrent client: Qt 6 Widgets UI styled by the ro-qss kit, on a backend
ported from qBittorrent's `src/base` (libtorrent). GPL-2.0-or-later. See docs/STATUS.md.

Backend-only check on any host:
    cmake -S . -B build -G Ninja -DROTORRENT_BACKEND_ONLY=ON -DROTORRENT_BUILD_TESTS=ON
    cmake --build build
