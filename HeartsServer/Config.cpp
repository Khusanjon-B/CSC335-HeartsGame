// ---------------------------------------------------------------------------
// Config.cpp - see Config.h.
//
// QSettings in IniFormat reads and writes plain "key=value" files grouped
// into [sections]. settings.value("lockout/maxFailedLogins", default) reads
// the key maxFailedLogins in section [lockout], or returns the default.
// ---------------------------------------------------------------------------

#include "Config.h"
#include "Log.h"

#include <QFileInfo>
#include <QSettings>

// Reads one positive whole number, falling back to `fallback` if the value
// is missing, not a number, or less than 1.
static int readPositiveInt(QSettings& s, const QString& key, int fallback)
{
    bool ok = false;
    const int value = s.value(key, fallback).toInt(&ok);
    if (!ok || value < 1) {
        logMessage("[Config] bad value for " + key + ", using default " + QString::number(fallback));
        return fallback;
    }
    return value;
}

ServerConfig ServerConfig::load(const QString& filePath)
{
    const bool existed = QFileInfo::exists(filePath);
    QSettings settings(filePath, QSettings::IniFormat);
    ServerConfig defaults;
    ServerConfig cfg;

    cfg.port               = static_cast<quint16>(readPositiveInt(settings, "server/port", defaults.port));
    cfg.maxFailedLogins    = readPositiveInt(settings, "lockout/maxFailedLogins", defaults.maxFailedLogins);
    cfg.lockoutMinutes     = readPositiveInt(settings, "lockout/lockoutMinutes", defaults.lockoutMinutes);
    cfg.maxConcurrentGames = readPositiveInt(settings, "games/maxConcurrentGames", defaults.maxConcurrentGames);

    // First run: write the file so there's something to edit.
    if (!existed) {
        settings.setValue("server/port", cfg.port);
        settings.setValue("lockout/maxFailedLogins", cfg.maxFailedLogins);
        settings.setValue("lockout/lockoutMinutes", cfg.lockoutMinutes);
        settings.setValue("games/maxConcurrentGames", cfg.maxConcurrentGames);
        settings.sync();
        logMessage("[Config] created " + filePath + " with default settings");
    } else {
        logMessage("[Config] loaded " + filePath);
    }

    logMessage(QString("[Config] port=%1 maxFailedLogins=%2 lockoutMinutes=%3 maxConcurrentGames=%4")
                   .arg(cfg.port).arg(cfg.maxFailedLogins).arg(cfg.lockoutMinutes)
                   .arg(cfg.maxConcurrentGames));
    return cfg;
}
