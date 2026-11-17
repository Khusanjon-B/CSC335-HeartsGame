#pragma once
// ---------------------------------------------------------------------------
// Log.h - timestamped logging to the terminal AND to a log file.
//
//   Terminal:  23:41:07  connected     192.168.1.20:53211
//   File:      2026-11-16 23:41:07  connected     192.168.1.20:53211
//
// main() calls initLogFile() once at startup. After that, every logMessage()
// goes to both places. If the file can't be opened, messages still go to
// the terminal.
//
// The file is opened in "append" mode, so restarting the server on the same
// day adds to that day's file instead of wiping it. Each line is flushed to
// disk immediately, so nothing is lost if the server crashes.
//
// A mutex makes it safe to call from any thread.
// ---------------------------------------------------------------------------

#include <QString>

// Opens (or creates) the log file, creating its folder if needed.
// Returns false if it couldn't be opened.
bool initLogFile(const QString& filePath);

// Prints one timestamped line to the terminal and the log file.
void logMessage(const QString& msg);
