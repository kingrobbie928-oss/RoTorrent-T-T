// SPDX-License-Identifier: GPL-2.0-or-later
// Licensed GPL-2.0-or-later.
// Catches SIGSEGV / SIGABRT / SIGFPE / SIGILL, writes the bug-report text to stderr and, in the GUI
// build, shows the Crash info dialog before letting the process die with the original signal.
// Only built when ROTORRENT_STACKTRACE is on.

#pragma once

// Installs the handlers. Call once, early, after the QApplication exists.
void registerCrashHandlers();

// Environment variable that skips the dialog (CI, tests, scripted runs). The text still goes to stderr.
#define ROTORRENT_NO_CRASH_DIALOG_ENV "ROTORRENT_NO_CRASH_DIALOG"
