@echo off
rem Optional. Adds RoTorrent to Windows' "Default apps" list so it can be chosen for .torrent files and magnet:
rem links. Per-user keys only (HKEY_CURRENT_USER), no administrator rights. Windows does not let a program make itself
rem the default: after running this, pick RoTorrent in Settings > Apps > Default apps (the button
rem Options > Behavior > "Open default apps" opens that page). Undo with Unregister-RoTorrent.bat.
rem If you move this folder, run Unregister-RoTorrent.bat first, then run this again from the new place.
setlocal
set "DIR=%~dp0"
if "%DIR:~-1%"=="\" set "DIR=%DIR:~0,-1%"
if not exist "%DIR%\rotorrent.exe" (echo rotorrent.exe not found next to this script. & pause & exit /b 1)
powershell -NoProfile -Command "(Get-Content -Raw -LiteralPath '%DIR%\extras\rotorrent-associations.reg.in').Replace('@INSTALL_DIR@', ('%DIR%' -replace '\\','\\')) | Set-Content -Encoding Unicode -LiteralPath '%TEMP%\rotorrent-associations.reg'"
if errorlevel 1 (echo Could not prepare the registry file. & pause & exit /b 1)
reg import "%TEMP%\rotorrent-associations.reg"
if errorlevel 1 (echo Registration failed. & pause & exit /b 1)
del "%TEMP%\rotorrent-associations.reg" 2>nul
echo RoTorrent is now listed in Windows Default apps. Opening the settings page...
start "" ms-settings:defaultapps
endlocal
