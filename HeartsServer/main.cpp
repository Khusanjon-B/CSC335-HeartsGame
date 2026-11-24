// ---------------------------------------------------------------------------
// main.cpp - Hearts server entry point.
//
// Usage: test_server [port]   (default: the port in server.ini, normally 5000)
//
// What main does:
//   1. Opens the log file (logs/server-YYYY-MM-DD.log next to the .exe)
//      and reads settings from server.ini (see Config.h).
//   2. Opens the SQLite database (hearts.db next to the .exe).
//   3. Starts the TCP Server.
//   4. Opens the Server Monitoring GUI window (MonitorWindow).
//   5. Starts a console thread so you can type broadcasts or "exit".
//   6. Runs Qt's event loop, which handles all network traffic AND the
//      window's clicks/redraws.
//
// The terminal is the log view; the window is the admin view. Closing the
// window or typing "exit" in the terminal both shut the server down.
//
// Files:
//   Log.h/cpp            - logMessage(): terminal + log file
//   Config.h/cpp         - server.ini settings (port, lockout, max games)
//   Message.h            - struct Message, requestId rules, secret redaction
//   Connection.h         - struct Connection: one connected client
//   PasswordHasher.h/cpp - salted SHA-256 for passwords and pins
//   Database.h/cpp       - all SQLite access (players table)
//   ErrorCodes.h         - every error code the server can send
//   Server.h/cpp         - TCP server, JSON parsing, command handlers
//   MonitorWindow.h/cpp  - Server Monitoring GUI (admin window)
//
// THREADS: only two.
//   - Main thread: Qt event loop. ALL networking, ALL database work and
//     the GUI window.
//   - Console thread: blocks on std::getline (so it can't run on the main
//     thread). It never touches server data; it hands work to the main
//     thread with QMetaObject::invokeMethod, so nothing needs a lock.
// ---------------------------------------------------------------------------

#include "Config.h"
#include "Database.h"
#include "Log.h"
#include "MonitorWindow.h"
#include "Server.h"

#include <QApplication>
#include <QDate>
#include <iostream>
#include <string>
#include <thread>

// Reads lines typed into the server's terminal on a background thread.
// "exit" quits the server; anything else is broadcast to all clients.
static void startConsoleThread(QApplication& app, Server& server)
{
    std::thread([&app, &server] {
        std::string input;
        while (std::getline(std::cin, input)) {
            const QString text = QString::fromStdString(input).trimmed();
            if (text.isEmpty())
                continue;

            if (text == "exit") {
                // Ask the main thread to stop the event loop.
                QMetaObject::invokeMethod(&app, &QCoreApplication::quit, Qt::QueuedConnection);
                return;
            }

            // Don't call server.broadcast() from this thread. invokeMethod
            // queues the lambda, and the main thread runs it.
            QMetaObject::invokeMethod(&server, [&server, text] {
                server.broadcast(text);
            }, Qt::QueuedConnection);
        }
    }).detach();
    // detach(): we never wait for this thread. It's stuck in getline, so
    // waiting could hang forever. The OS cleans it up when the process exits.
}

int main(int argc, char* argv[])
{
    // QApplication (instead of QCoreApplication) is needed for windows.
    // It's still the same event loop that drives the network code.
    QApplication app(argc, argv);
    const QString exeDir = QCoreApplication::applicationDirPath();

    // ---- log file: one per day, e.g. logs/server-2026-11-16.log ----
    const QString logPath = exeDir + "/logs/server-" +
                            QDate::currentDate().toString("yyyy-MM-dd") + ".log";
    if (initLogFile(logPath))
        logMessage("===== server starting, logging to " + logPath + " =====");
    else
        logMessage("could not open log file " + logPath + " (logging to terminal only)");

    // ---- settings from server.ini (created with defaults on first run) ----
    ServerConfig config = ServerConfig::load(exeDir + "/server.ini");

    // ---- port: server.ini, unless one is given on the command line ----
    quint16 port = config.port;
    if (argc > 1) {
        bool ok = false;
        port = QString(argv[1]).toUShort(&ok);
        if (!ok || port == 0) {
            logMessage("invalid port: " + QString(argv[1]));
            return 1;
        }
    }

    // ---- database (inside the build folder, which git ignores) ----
    Database db;
    if (!db.open(exeDir + "/hearts.db"))
        return 1;
    db.resetAllConnected();   // nobody is connected to a server that just started

    // ---- network server ----
    Server server(db, config);
    if (!server.start(port))
        return 1;

    // Right before the event loop stops (e.g. after "exit"), log everyone out
    // so the DB stays accurate.
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &server, &Server::logoutEveryone);

    // ---- Server Monitoring GUI ----
    // Created after the server so it can read live data from it right away.
    MonitorWindow monitor(server);
    monitor.show();

    startConsoleThread(app, server);

    logMessage("type a message to broadcast it, or \"exit\" to quit");
    return app.exec();   // handles events until quit()
}
