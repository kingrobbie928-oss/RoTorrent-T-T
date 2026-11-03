// SPDX-License-Identifier: GPL-2.0-or-later
// Builds the main window on a live BitTorrent session with every dialog wired in. Shared by the real
// rotorrent executable (app/application.cpp) and rotorrent_window --engine. Licensed GPL-2.0-or-later.

#pragma once

class IApplication;
class MainWindow;

namespace BitTorrent
{
    class Session;
}

namespace EngineWindow
{
    // Returns a window that is not shown yet. It owns the list adapter, the controller and the details
    // source, so it must be deleted before the session is freed. The caller shows it.
    // `app` supplies Options (file logger, instance name, memory limits); without it the Options hook stays a
    // "not available" box (the rotorrent_window preview).
    MainWindow *create(BitTorrent::Session *session, IApplication *app = nullptr);
}
