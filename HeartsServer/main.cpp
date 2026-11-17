// ---------------------------------------------------------------------------
// main.cpp - Hearts server entry point.
//
// Usage: test_server [port]   (default 5000)
//
// What main does:
//   1. Opens the log file (logs/server-YYYY-MM-DD.log next to the .exe).
//   2. Opens the SQLite database (hearts.db next to the .exe).
//   3. Starts the TCP Server.
//   4. Starts a console thread so you can type broadcasts or "exit".
//   5. Runs Qt's event loop, which handles all network traffic.
//
// Files:
//   Log.h/cpp            - logMessage(): terminal + log file
//   Message.h            - struct Message, requestId rules, secret redaction
//   Connection.h         - struct Connection: one connected client
//   PasswordHasher.h/cpp - salted SHA-256 for passwords and pins
//   Database.h/cpp       - all SQLite access (players table)
//   Server.h/cpp         - TCP server, JSON parsing, command handlers
//
// THREADS: only two.
//   - Main thread: Qt event loop. ALL networking and ALL database work.
//   - Console thread: blocks on std::getline (so it can't run on the main
//     thread). It never touches server data; it hands work to the main
//     thread with QMetaObject::invokeMethod, so nothing needs a lock.
// ---------------------------------------------------------------------------

#include "Database.h"
#include "Log.h"
#include "Server.h"

#include <QCoreApplication>
#include <QDate>
#include <iostream>
#include <string>
#include <thread>

// Reads lines typed into the server's terminal on a background thread.
// "exit" quits the server; anything else is broadcast to all clients.
static void startConsoleThread(QCoreApplication& app, Server& server)
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
    QCoreApplication app(argc, argv);
    const QString exeDir = QCoreApplication::applicationDirPath();

    // ---- log file: one per day, e.g. logs/server-2026-11-16.log ----
    const QString logPath = exeDir + "/logs/server-" +
                            QDate::currentDate().toString("yyyy-MM-dd") + ".log";
    if (initLogFile(logPath))
        logMessage("===== server starting, logging to " + logPath + " =====");
    else
        logMessage("could not open log file " + logPath + " (logging to terminal only)");

    // ---- port from the command line ----
    quint16 port = 5000;
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
    Server server(db);
    if (!server.start(port))
        return 1;

    // Right before the event loop stops (e.g. after "exit"), log everyone out
    // so the DB stays accurate.
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &server, &Server::logoutEveryone);

    startConsoleThread(app, server);

    logMessage("type a message to broadcast it, or \"exit\" to quit");
    return app.exec();   // handles events until quit()
}
