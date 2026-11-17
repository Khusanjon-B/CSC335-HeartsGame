// ---------------------------------------------------------------------------
// Log.cpp - see Log.h.
// ---------------------------------------------------------------------------

#include "Log.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>

// An anonymous namespace makes these private to this file. Everything else
// goes through logMessage().
namespace {
QMutex logMutex;   // one thread writes a line at a time
QFile  logFile;    // closed until initLogFile() succeeds
}

bool initLogFile(const QString& filePath)
{
    QMutexLocker lock(&logMutex);   // unlocks automatically at the closing brace

    QDir().mkpath(QFileInfo(filePath).absolutePath());   // e.g. create build/logs
    logFile.setFileName(filePath);
    return logFile.open(QIODevice::Append | QIODevice::Text);
}

void logMessage(const QString& msg)
{
    QMutexLocker lock(&logMutex);
    const QDateTime now = QDateTime::currentDateTime();

    // Terminal: time only, easier to read while watching live.
    static QTextStream console(stdout);
    console << now.toString("hh:mm:ss") << "  " << msg << Qt::endl;

    // File: date and time, since a file can cover a long session.
    if (logFile.isOpen()) {
        logFile.write((now.toString("yyyy-MM-dd hh:mm:ss") + "  " + msg + "\n").toUtf8());
        logFile.flush();
    }
}
