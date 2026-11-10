#pragma once
// ---------------------------------------------------------------------------
// Log.h - one tiny helper used by every file to print timestamped lines.
//
// Example output:   23:41:07  connected     192.168.1.20:53211
//
// It's "inline" so it can live in a header and be included from many .cpp
// files without the linker complaining about duplicate definitions.
// (It's called logMessage instead of log so it can't clash with the math
//  function log() from <cmath>.)
// ---------------------------------------------------------------------------

#include <QDateTime>
#include <QString>
#include <QTextStream>

inline void logMessage(const QString& msg)
{
    // Created once, the first time logMessage is called, then reused.
    static QTextStream out(stdout);
    out << QDateTime::currentDateTime().toString("hh:mm:ss") << "  " << msg << Qt::endl;
}
