# Release procedure (Phase 9)

Everything below runs on Windows 10/11 x64 with MSVC 2022, Qt 6.6 or newer, CMake and vcpkg (libtorrent 2.0.10+, Boost,
OpenSSL 3, zlib). Nothing here has been run yet: the scripts were written and syntax-checked on Linux only.

## 1. Gates that must be green
```
sh test/run_all.sh <build-dir>          # every automated check (needs a Qt platform that works headless)
RELEASE=1 sh test/check_placeholders.sh # fails while any placeholder remains (docs/PLACEHOLDERS.md)
sh test/check_translations.sh           # translations/rotorrent_en.ts matches the sources (run with --update after UI text changes)
```
Then the manual matrix in `docs/QA.md` (Phase 9 section). **No open P1/P2 defect** allowed.

## 2. Version
Set it in one place: `project(RoTorrent VERSION x.y.z)` in `CMakeLists.txt`. `src/base/version.h` and the Windows
resource (file properties) follow it. For a pre-release configure with `-DROTORRENT_VERSION_STATUS=rc1`.
Update `dist/windows/rotorrent.manifest` `assemblyIdentity version` to match (the only copy not generated).

## 3. Build
```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build build
```
Keep the `.pdb` next to `rotorrent.exe` while testing: the crash dialog reads it. Do not ship it.

## 4. Package
```
pwsh dist/windows/package.ps1 -BuildDir build -QtBin <Qt>\bin -SourceUrl <public repo URL> [-SignCert <thumbprint>]
```
Produces `release/RoTorrent-<ver>-win64-portable.zip`, `RoTorrent-<ver>-source.zip`, `SHA256SUMS.txt` and starts the staged
build once as a smoke check. Set `VCPKG_INSTALLED_DIR` first so the third-party licence texts are copied.

## 5. Optional installer
```
makensis /DVERSION=<ver> /DSTAGE=release\RoTorrent-<ver>-win64-portable dist\windows\rotorrent.nsi
```
Per-user, no admin rights. It compiled with `makensis` on Linux against a dummy folder; it has not run on Windows.

## 6. Notices that are NOT automated (do by hand)
- **Qt (LGPL):** put the Qt licence text and a written offer for the Qt source (or a link to the exact Qt version's source) in
  `licenses/`. RoTorrent links Qt dynamically; do not replace the Qt DLLs with static ones without re-checking.
- **libtorrent, Boost, OpenSSL, zlib:** `package.ps1` copies vcpkg's `copyright` files when `VCPKG_INSTALLED_DIR` is set.
  Check that all four are present in `licenses/`.
- **DB-IP:** attribution is in About > Software Used (CC BY 4.0). Keep it.
- **Icons, logos:** confirm the owner may license them under GPL-compatible terms (THIRD_PARTY.md).

## 7. Clean-machine test
Fresh Windows 10 and 11 VMs: unzip, run `RoTorrent-portable.bat`, add a torrent, close and reopen, delete the folder. Then
run the installer and uninstall it (settings in `%APPDATA%\RoTorrent` are kept on purpose).

## 8. Publish
Upload the portable zip, the source zip and `SHA256SUMS.txt` together; link the source repository from About and the
website. Tag the exact commit. SmartScreen warns about unsigned builds until a certificate or reputation exists.
