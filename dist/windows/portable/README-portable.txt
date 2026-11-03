RoTorrent @VERSION@ (portable, Windows 10 / 11, 64-bit)
=====================================================

How to run
  1. Unzip this folder anywhere you can write to (not "Program Files").
  2. Double-click RoTorrent-portable.bat.
     Everything RoTorrent stores goes into the "profile" folder beside it. Deleting the folder removes it all
     (if you used Register-RoTorrent.bat below, run Unregister-RoTorrent.bat first).
  Running rotorrent.exe directly also works, but then settings go to your user profile (%APPDATA% / %LOCALAPPDATA%)
  instead of the portable "profile" folder.

Your existing torrent client is never touched. RoTorrent uses its own settings and data; it does not read or
write another program's profile.

First start
  RoTorrent shows a short legal notice once. Windows SmartScreen may warn about an unsigned download; "More info"
  then "Run anyway" starts it. (Release builds signed with a certificate do not show this.)

Opening .torrent files and magnet links
  Unzipped, RoTorrent does not change any Windows setting, so Windows does not offer it for .torrent files or
  magnet links yet. Two optional scripts handle that:
    Register-RoTorrent.bat     adds RoTorrent to Settings > Apps > Default apps (current user only, no admin
                               rights), then opens that page so you can choose it. Windows itself does not let a
                               program make itself the default.
    Unregister-RoTorrent.bat   removes what the first script added.
  Alternatively drag a .torrent file onto the RoTorrent window, or use File > Add Torrent File / Add Torrent Link.

Licence
  RoTorrent is free software, GNU GPL version 2 or (at your option) any later version. It is based on the
  qBittorrent project. The full licence texts and the notices of every bundled component are in the "licenses"
  folder. The complete corresponding source code is available as described in SOURCE-OFFER.txt.
