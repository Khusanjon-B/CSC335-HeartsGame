#pragma once
// ---------------------------------------------------------------------------
// Config.h - settings read from server.ini (next to the .exe).
//
// The team notes (9/29, 10/6) say values like the login-attempt limit and
// the lock delay belong in an external config file, so they can be changed
// without recompiling. This is that file.
//
// If server.ini doesn't exist, it's created with the defaults below, so
// there's always a file to edit. Example:
//
//   [server]
//   port=5000
//
//   [lockout]
//   maxFailedLogins=3
//   lockoutMinutes=5
//
//   [games]
//   maxConcurrentGames=1
//
// A port given on the command line (test_server 6000) overrides the file.
// To add a setting: add a field here with its default, then one line each
// in Config.cpp to read and write it.
// ---------------------------------------------------------------------------

#include <QString>

struct ServerConfig
{
    quint16 port               = 5000;  // TCP port clients connect to
    int     maxFailedLogins    = 3;     // wrong passwords in a row before lockout (UC 7)
    int     lockoutMinutes     = 5;     // how long a lockout lasts (UC 7)
    int     maxConcurrentGames = 1;     // lobbies/games allowed at once (UC 11)

    // Reads the file (creating it with defaults if it's missing).
    // Bad or missing values fall back to the defaults above.
    static ServerConfig load(const QString& filePath);
};
