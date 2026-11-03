; RoTorrent installer (optional deliverable; the portable zip is the required one).
; Build:  makensis /DVERSION=1.0.0 /DSTAGE=<path to the staged portable folder> rotorrent.nsi
;   STAGE is the RoTorrent-<ver>-win64-portable folder that package.ps1 produces (without the portable-only
;   launcher / register scripts, which the installer ignores).
; Per-user install (no administrator rights) into %LOCALAPPDATA%\Programs\RoTorrent.
; Licensed GPL-2.0-or-later like the rest of the project. Written for RoTorrent; not copied from another installer.
Unicode true
!ifndef VERSION
  !error "Pass /DVERSION=x.y.z"
!endif
!ifndef STAGE
  !error "Pass /DSTAGE=<staged portable folder>"
!endif

!include "MUI2.nsh"
!include "FileFunc.nsh"

Name "RoTorrent ${VERSION}"
OutFile "RoTorrent-${VERSION}-win64-setup.exe"
InstallDir "$LOCALAPPDATA\Programs\RoTorrent"
InstallDirRegKey HKCU "Software\RoTorrent" "InstallDir"
RequestExecutionLevel user
SetCompressor /SOLID lzma
BrandingText "RoTorrent ${VERSION}"

!define MUI_ICON "rotorrent.ico"
!define MUI_UNICON "rotorrent.ico"
!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$INSTDIR\rotorrent.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Start RoTorrent"

!insertmacro MUI_PAGE_LICENSE "${STAGE}\licenses\COPYING.GPLv2"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Section "RoTorrent" SecMain
  SectionIn RO
  SetOutPath "$INSTDIR"
  ; Everything in the staged folder except the portable-only files.
  File /r /x "RoTorrent-portable.bat" /x "Register-RoTorrent.bat" /x "Unregister-RoTorrent.bat" /x "extras" "${STAGE}\*.*"
  WriteUninstaller "$INSTDIR\uninstall.exe"
  WriteRegStr HKCU "Software\RoTorrent" "InstallDir" "$INSTDIR"

  ; Apps & Features entry (per user)
  !define UNINST "Software\Microsoft\Windows\CurrentVersion\Uninstall\RoTorrent"
  WriteRegStr   HKCU "${UNINST}" "DisplayName" "RoTorrent"
  WriteRegStr   HKCU "${UNINST}" "DisplayVersion" "${VERSION}"
  WriteRegStr   HKCU "${UNINST}" "Publisher" "The RoTorrent project"
  WriteRegStr   HKCU "${UNINST}" "DisplayIcon" "$INSTDIR\rotorrent.exe"
  WriteRegStr   HKCU "${UNINST}" "InstallLocation" "$INSTDIR"
  WriteRegStr   HKCU "${UNINST}" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKCU "${UNINST}" "NoModify" 1
  WriteRegDWORD HKCU "${UNINST}" "NoRepair" 1
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD HKCU "${UNINST}" "EstimatedSize" "$0"
SectionEnd

Section "Start menu shortcut" SecStart
  SectionIn RO
  CreateShortcut "$SMPROGRAMS\RoTorrent.lnk" "$INSTDIR\rotorrent.exe"
SectionEnd

Section "Offer RoTorrent for .torrent files and magnet links" SecAssoc
  ; Same keys as dist/windows/rotorrent-associations.reg.in. Windows does not let a program make itself the default;
  ; these make RoTorrent appear in Settings > Apps > Default apps.
  WriteRegStr HKCU "Software\Classes\RoTorrent.torrent" "" "BitTorrent file"
  WriteRegStr HKCU "Software\Classes\RoTorrent.torrent" "FriendlyTypeName" "BitTorrent file"
  WriteRegStr HKCU "Software\Classes\RoTorrent.torrent\DefaultIcon" "" '"$INSTDIR\rotorrent.exe",0'
  WriteRegStr HKCU "Software\Classes\RoTorrent.torrent\shell\open\command" "" '"$INSTDIR\rotorrent.exe" "%1"'
  WriteRegStr HKCU "Software\Classes\.torrent\OpenWithProgids" "RoTorrent.torrent" ""
  WriteRegStr HKCU "Software\Classes\RoTorrent.magnet" "" "URL:Magnet link"
  WriteRegStr HKCU "Software\Classes\RoTorrent.magnet" "URL Protocol" ""
  WriteRegStr HKCU "Software\Classes\RoTorrent.magnet\DefaultIcon" "" '"$INSTDIR\rotorrent.exe",0'
  WriteRegStr HKCU "Software\Classes\RoTorrent.magnet\shell\open\command" "" '"$INSTDIR\rotorrent.exe" "%1"'
  WriteRegStr HKCU "Software\RoTorrent\Capabilities" "ApplicationName" "RoTorrent"
  WriteRegStr HKCU "Software\RoTorrent\Capabilities" "ApplicationDescription" "A fast, modern BitTorrent client."
  WriteRegStr HKCU "Software\RoTorrent\Capabilities\FileAssociations" ".torrent" "RoTorrent.torrent"
  WriteRegStr HKCU "Software\RoTorrent\Capabilities\URLAssociations" "magnet" "RoTorrent.magnet"
  WriteRegStr HKCU "Software\RegisteredApplications" "RoTorrent" "Software\RoTorrent\Capabilities"
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecMain} "The RoTorrent program and its licence files."
  !insertmacro MUI_DESCRIPTION_TEXT ${SecStart} "Adds RoTorrent to the Start menu."
  !insertmacro MUI_DESCRIPTION_TEXT ${SecAssoc} "Lists RoTorrent in Windows' Default apps for .torrent files and magnet links (current user only)."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Section "Uninstall"
  ; Program files and shortcut. The user's settings and torrents (%APPDATA%\RoTorrent, %LOCALAPPDATA%\RoTorrent) are
  ; deliberately kept, so reinstalling restores everything.
  Delete "$SMPROGRAMS\RoTorrent.lnk"
  RMDir /r "$INSTDIR"
  DeleteRegKey HKCU "Software\Classes\RoTorrent.torrent"
  DeleteRegKey HKCU "Software\Classes\RoTorrent.magnet"
  DeleteRegValue HKCU "Software\Classes\.torrent\OpenWithProgids" "RoTorrent.torrent"
  DeleteRegValue HKCU "Software\RegisteredApplications" "RoTorrent"
  DeleteRegKey HKCU "Software\RoTorrent"
  DeleteRegKey HKCU "${UNINST}"
SectionEnd
