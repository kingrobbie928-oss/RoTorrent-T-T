@echo off
rem Removes everything Register-RoTorrent.bat added (HKEY_CURRENT_USER only). Safe to run when nothing is registered.
reg delete "HKCU\Software\Classes\RoTorrent.torrent" /f >nul 2>&1
reg delete "HKCU\Software\Classes\RoTorrent.magnet" /f >nul 2>&1
reg delete "HKCU\Software\Classes\.torrent\OpenWithProgids" /v "RoTorrent.torrent" /f >nul 2>&1
reg delete "HKCU\Software\RoTorrent" /f >nul 2>&1
reg delete "HKCU\Software\RegisteredApplications" /v "RoTorrent" /f >nul 2>&1
echo RoTorrent registrations removed.
