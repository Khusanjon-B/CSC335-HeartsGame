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
    cfg.gameOverScore      = readPositiveInt(settings, "games/gameOverScore", defaults.gameOverScore);

    // Write any setting that's missing from the file (all of them on the
    // first run), so every setting is there to edit, including ones added
    // in a newer version of the server.
    const std::pair<QString, int> all[] = {
        {"server/port",               cfg.port},
        {"lockout/maxFailedLogins",   cfg.maxFailedLogins},
        {"lockout/lockoutMinutes",    cfg.lockoutMinutes},
        {"games/maxConcurrentGames",  cfg.maxConcurrentGames},
        {"games/gameOverScore",       cfg.gameOverScore},
    };
    for (const auto& [key, value] : all)
        if (!settings.contains(key))
            settings.setValue(key, value);
    settings.sync();
    logMessage(existed ? "[Config] loaded " + filePath
                       : "[Config] created " + filePath + " with default settings");

    logMessage(QString("[Config] port=%1 maxFailedLogins=%2 lockoutMinutes=%3 maxConcurrentGames=%4 "
                       "gameOverScore=%5")
                   .arg(cfg.port).arg(cfg.maxFailedLogins).arg(cfg.lockoutMinutes)
                   .arg(cfg.maxConcurrentGames).arg(cfg.gameOverScore));
    return cfg;
}
