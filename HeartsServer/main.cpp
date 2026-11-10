// ---------------------------------------------------------------------------
// main.cpp - Hearts server entry point.
//
// Usage: test_server [port]   (default 5000)
//
// What main does:
//   1. Opens the SQLite database (hearts.db next to the .exe).
//   2. Starts the TCP Server.
//   3. Starts a console thread so you can type broadcasts or "exit".
//   4. Runs Qt's event loop, which handles all network traffic.
//
// Files:
//   Log.h          - logMessage() helper
//   Connection.h   - the struct describing one connected client
//   Database.h/cpp - all SQLite access (players table)
//   Server.h/cpp   - TCP server, JSON parsing, login/logout/register
//
// THREADS: there are only two.
//   - Main thread: Qt event loop. Runs ALL networking and ALL database work.
//   - Console thread: waits on std::getline (which blocks, so it can't run
//     on the main thread). It never touches the server's data directly. It
//     hands work to the main thread with QMetaObject::invokeMethod, so
//     there's still nothing that needs a lock.
// ---------------------------------------------------------------------------

#include "Database.h"
#include "Log.h"
#include "Server.h"

#include <QCoreApplication>
#include <iostream>
#include <string>
#include <thread>

// Reads lines typed into the server's terminal, forever, on a background
// thread. "exit" quits the server. Anything else is broadcast to all clients.
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

            // Don't call server.broadcast() from here; this is the wrong
            // thread. invokeMethod queues the lambda, and the main thread
            // runs it on its next pass through the event loop.
            QMetaObject::invokeMethod(&server, [&server, text] {
                server.broadcast(text);
            }, Qt::QueuedConnection);
        }
    }).detach();
    // detach(): we never join this thread. It's stuck inside getline, so a
    // join could hang forever. The OS cleans it up when the process exits.
    // (The old code never joined or detached it, which makes C++ call
    //  std::terminate when main returns.)
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

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

    // ---- database ----
    // Stored next to the executable (inside the build folder, which git
    // ignores). Delete the file to start over with an empty database.
    Database db;
    if (!db.open(QCoreApplication::applicationDirPath() + "/hearts.db"))
        return 1;
    db.resetAllConnected();   // nobody is connected to a server that just started

    // ---- network server ----
    Server server(db);
    if (!server.start(port))
        return 1;

    // When the event loop is about to stop (e.g. after "exit"), record a
    // logout for everyone still logged in so the DB stays accurate.
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &server, &Server::logoutEveryone);

    startConsoleThread(app, server);

    logMessage("type a message to broadcast it, or \"exit\" to quit");
    return app.exec();   // blocks here, handling events, until quit()
}
