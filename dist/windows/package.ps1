<#
 RoTorrent release packager (handoff phase 9). Run on Windows, in a Developer PowerShell for VS 2022, after a
 Release build:

   cmake --preset ...  /  cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release   (vcpkg toolchain)
   cmake --build build --config Release
   pwsh dist/windows/package.ps1 -BuildDir build -QtBin C:\Qt\6.x.y\msvc2022_64\bin

 Produces, in <OutDir> (default ./release):
   RoTorrent-<ver>-win64-portable.zip   the portable build (the required deliverable)
   RoTorrent-<ver>-source.zip           complete corresponding source (GPL)
   SHA256SUMS.txt
 and verifies the staged portable folder actually starts.

 Parameters
   -BuildDir   CMake build directory that contains rotorrent.exe (and the DLLs vcpkg copied beside it).
   -QtBin      Qt "bin" folder holding windeployqt.exe.
   -OutDir     Where to write the results. Default: ./release
   -SourceUrl  Public source repository URL written into SOURCE-OFFER.txt.
   -SignCert   Optional code-signing certificate thumbprint; signs rotorrent.exe when given.
   -SkipSmoke  Skip the "does it start" check (for a headless CI machine).
#>
param(
    [Parameter(Mandatory)][string]$BuildDir,
    [Parameter(Mandatory)][string]$QtBin,
    [string]$OutDir = "release",
    [string]$SourceUrl = "https://rotorrent.example/source",
    [string]$SignCert = "",
    [switch]$SkipSmoke
)
$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$build = Resolve-Path $BuildDir

# --- version: the single source is project(VERSION) in CMakeLists.txt ---
$cm = Get-Content (Join-Path $root "CMakeLists.txt") -Raw
if ($cm -notmatch 'VERSION\s+(\d+\.\d+\.\d+)') { throw "Cannot read the version from CMakeLists.txt" }
$ver = $Matches[1]
$status = ""
if ((Get-Content (Join-Path $root "src/base/version.h") -Raw) -match 'QBT_VERSION_STATUS\s+"([^"]*)"') { $status = $Matches[1] }
$full = "$ver$status"
Write-Host "RoTorrent $full"

$exe = Join-Path $build "rotorrent.exe"
if (-not (Test-Path $exe)) { throw "rotorrent.exe not found in $build" }

# --- stage ---
New-Item -ItemType Directory -Force $OutDir | Out-Null
$out = Resolve-Path $OutDir
$name = "RoTorrent-$full-win64-portable"
$stage = Join-Path $out $name
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory $stage | Out-Null
Copy-Item $exe $stage

# --- code signing (optional) ---
if ($SignCert) {
    & signtool sign /sha1 $SignCert /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 (Join-Path $stage "rotorrent.exe")
    if ($LASTEXITCODE -ne 0) { throw "signtool failed" }
}

# --- Qt runtime: DLLs, platform / imageformats / iconengines / sqldrivers / tls plugins ---
& (Join-Path $QtBin "windeployqt.exe") --release --no-translations --no-system-d3d-compiler --no-opengl-sw `
    --dir $stage (Join-Path $stage "rotorrent.exe")
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed" }

# --- non-Qt DLLs (libtorrent, OpenSSL, zlib, ...): vcpkg's app-local step already put them next to rotorrent.exe
#     in the build folder. Copy every DLL that is not already staged.
Get-ChildItem $build -Filter *.dll | Where-Object { -not (Test-Path (Join-Path $stage $_.Name)) } |
    ForEach-Object { Copy-Item $_.FullName $stage }

# --- launcher, readme, licences ---
Copy-Item (Join-Path $PSScriptRoot "portable\RoTorrent-portable.bat") $stage
Copy-Item (Join-Path $PSScriptRoot "portable\Register-RoTorrent.bat") $stage
Copy-Item (Join-Path $PSScriptRoot "portable\Unregister-RoTorrent.bat") $stage
New-Item -ItemType Directory (Join-Path $stage "extras") | Out-Null
Copy-Item (Join-Path $PSScriptRoot "rotorrent-associations.reg.in") (Join-Path $stage "extras")
(Get-Content (Join-Path $PSScriptRoot "portable\README-portable.txt") -Raw).Replace("@VERSION@", $full) |
    Set-Content (Join-Path $stage "README.txt") -Encoding UTF8
(Get-Content (Join-Path $PSScriptRoot "portable\SOURCE-OFFER.txt") -Raw).Replace("@VERSION@", $full).Replace("@SOURCE_URL@", $SourceUrl) |
    Set-Content (Join-Path $stage "SOURCE-OFFER.txt") -Encoding UTF8
$lic = Join-Path $stage "licenses"
New-Item -ItemType Directory $lic | Out-Null
foreach ($f in "COPYING", "COPYING.GPLv2", "COPYING.GPLv3", "AUTHORS") { Copy-Item (Join-Path $root $f) $lic }
Copy-Item (Join-Path $root "src/resources/ro/fonts/OFL.txt") (Join-Path $lic "NunitoSans-OFL.txt")
Copy-Item (Join-Path $root "docs/THIRD_PARTY.md") $lic
Copy-Item (Join-Path $root "docs/UPSTREAM.md") $lic
Copy-Item (Join-Path $root "docs/IDENTITY.md") $lic
# Third-party licence texts that the build machine provides (vcpkg keeps them under share/<port>/copyright).
$vc = $env:VCPKG_INSTALLED_DIR
if ($vc) {
    Get-ChildItem (Join-Path $vc "*\share\*\copyright") -ErrorAction SilentlyContinue | ForEach-Object {
        Copy-Item $_.FullName (Join-Path $lic ("{0}-copyright.txt" -f $_.Directory.Name))
    }
} else { Write-Warning "VCPKG_INSTALLED_DIR not set: third-party licence texts were not copied. Add them by hand before release." }
# Qt (LGPLv3): licence text plus the notice that points at the Qt source (docs/RELEASE.md step 6).
$qtVer = if ($QtBin -match 'Qt[\\/](\d+\.\d+\.\d+)') { $Matches[1] } else { '6.x' }
$qtSeries = ($qtVer -split '\.')[0..1] -join '.'
Copy-Item (Join-Path $PSScriptRoot 'licenses\Qt-LGPL-3.0.txt') $lic
(Get-Content (Join-Path $PSScriptRoot 'licenses\Qt-NOTICE.txt.in') -Raw).Replace('@QT_VERSION@', $qtVer).Replace('@QT_SERIES@', $qtSeries) | Set-Content (Join-Path $lic 'Qt-NOTICE.txt') -Encoding UTF8

# --- does it start? Run it with a throw-away profile; it must still be alive after 8 s. ---
if (-not $SkipSmoke) {
    $tmp = Join-Path ([IO.Path]::GetTempPath()) ("rt-smoke-" + [guid]::NewGuid())
    $p = Start-Process (Join-Path $stage "rotorrent.exe") -ArgumentList "--profile=`"$tmp`"", "--confirm-legal-notice" -PassThru
    Start-Sleep -Seconds 8
    $alive = -not $p.HasExited
    if ($alive) { Stop-Process $p -Force }
    Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue
    if (-not $alive) { throw "Smoke test failed: rotorrent.exe exited within 8 s (exit code $($p.ExitCode)). Missing DLL or plugin?" }
    Write-Host "Smoke test OK"
}

# --- portable zip ---
$zip = Join-Path $out "$name.zip"
if (Test-Path $zip) { Remove-Item $zip }
Compress-Archive -Path $stage -DestinationPath $zip

# --- source zip: the exact tree, without build output ---
$src = Join-Path $out "RoTorrent-$full-source.zip"
if (Test-Path $src) { Remove-Item $src }
if (Test-Path (Join-Path $root ".git")) {
    & git -C $root archive --format=zip --prefix="RoTorrent-$full/" -o $src HEAD
    if ($LASTEXITCODE -ne 0) { throw "git archive failed" }
} else {
    $tmpSrc = Join-Path $out "src-stage"
    if (Test-Path $tmpSrc) { Remove-Item -Recurse -Force $tmpSrc }
    robocopy $root (Join-Path $tmpSrc "RoTorrent-$full") /E /XD build build20 .git release out vcpkg_installed .vs /NFL /NDL /NJH /NJS | Out-Null
    Compress-Archive -Path (Join-Path $tmpSrc "RoTorrent-$full") -DestinationPath $src
    Remove-Item -Recurse -Force $tmpSrc
}

# --- checksums ---
Get-FileHash $zip, $src -Algorithm SHA256 | ForEach-Object { "{0}  {1}" -f $_.Hash.ToLower(), (Split-Path $_.Path -Leaf) } |
    Set-Content (Join-Path $out "SHA256SUMS.txt") -Encoding ASCII
Write-Host "Done. See $out"
