// SPDX-License-Identifier: GPL-2.0-or-later
// Licensed GPL-2.0-or-later.

#include "crashhandler.h"

#include <QtSystemDetection>

#include <csignal>
#include <cstdlib>
#include <cstring>
#include <string>
#include <tuple>

#ifdef Q_OS_UNIX
#include <unistd.h>
#elif defined Q_OS_WIN
#include <io.h>
#endif

#include <QString>

#ifndef DISABLE_GUI
#include <QApplication>
#include "gui/dialogs/StacktraceDialog.h"
#endif

#include "base/version.h"
#include "stacktrace.h"

namespace
{
    const char *signalName(const int signum)
    {
        switch (signum)
        {
        case SIGABRT: return "SIGABRT";
        case SIGSEGV: return "SIGSEGV";
        case SIGFPE: return "SIGFPE";
        case SIGILL: return "SIGILL";
        default: return "unknown signal";
        }
    }

    void safePrint(const char *str)
    {
        const std::size_t len = std::strlen(str);
#ifdef Q_OS_WIN
        if (_write(_fileno(stderr), str, static_cast<unsigned int>(len)) < static_cast<int>(len))
            std::ignore = _write(_fileno(stdout), str, static_cast<unsigned int>(len));
#else
        if (write(STDERR_FILENO, str, len) < static_cast<ssize_t>(len))
            std::ignore = write(STDOUT_FILENO, str, len);
#endif
    }

    void abnormalExitHandler(const int signum)
    {
        // A crash inside this handler must not loop: go back to the default action first.
        std::signal(signum, SIG_DFL);

        const char *name = signalName(signum);
        const std::string stacktrace = getStacktrace();

        const char header[] = "\n\n*************************************************************\n"
            "Please file a bug report at https://bugs.rotorrent.example and provide the following information:\n\n"
            "RoTorrent version: " QBT_VERSION "\n\n"
            "Caught signal: ";
        const char *parts[] = {header, name, "\n```\n", stacktrace.c_str(), "```\n\n"};
        for (const char *part : parts)
            safePrint(part);

#ifndef DISABLE_GUI
        // Unsafe by nature (a crashed process is drawing widgets), as in upstream; the text is already on stderr.
        if (qobject_cast<QApplication *>(QCoreApplication::instance()) && !std::getenv(ROTORRENT_NO_CRASH_DIALOG_ENV))
        {
            StacktraceDialog dialog;
            dialog.setText(QString::fromLatin1(name), QString::fromStdString(stacktrace));
            dialog.run();
        }
#endif

        std::raise(signum);
    }
}

void registerCrashHandlers()
{
    std::signal(SIGABRT, abnormalExitHandler);
    std::signal(SIGSEGV, abnormalExitHandler);
    std::signal(SIGFPE, abnormalExitHandler);
    std::signal(SIGILL, abnormalExitHandler);
}
