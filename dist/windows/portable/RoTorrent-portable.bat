@echo off
rem RoTorrent portable launcher: keeps settings, torrents and logs in the "profile" folder next to this file,
rem so the whole folder can live on a USB stick and a normal install is never touched.
rem Arguments (a .torrent file, a magnet link, options) are passed through to RoTorrent.
setlocal
start "" "%~dp0rotorrent.exe" --profile="%~dp0profile" %*
endlocal
