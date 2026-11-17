#pragma once
// ---------------------------------------------------------------------------
// Message.h - one JSON message (sent or received), plus two helpers.
//
// Every message is one JSON object per line, for example:
//   {"command":"login","requestId":12,"username":"alice","password":"pw"}
//
// REQUEST IDs
//   - Every message a client sends must include "requestId": a positive
//     whole number (1, 2, 3, ...) that the client picks, usually a counter.
//   - Every reply copies that requestId back, so the client knows which
//     request it answers.
//   - Messages the SERVER starts (like console broadcasts) use NEGATIVE ids
//     (-1, -2, ...) so they can never be confused with a reply.
//   - 0 means "unknown". The server uses it when replying to a message it
//     couldn't get an id from (broken JSON, missing id, ...).
// ---------------------------------------------------------------------------

#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QStringList>

struct Message
{
    enum class Direction { FromClient, ToClient };

    Direction   direction = Direction::FromClient;
    qint64      requestId = 0;   // see REQUEST IDs above
    QString     command;         // "login", "login_result", "error", ...
    QJsonObject body;            // the full JSON object
    QDateTime   time;            // when the server received or sent it
};

// Builds a Message from a JSON object, pulling out "command" and "requestId".
// requestId becomes 0 if it's missing, not a number, or not a whole number.
inline Message makeMessage(Message::Direction direction, const QJsonObject& body)
{
    Message m;
    m.direction = direction;
    m.body      = body;
    m.command   = body.value("command").toString();
    m.requestId = body.value("requestId").toInteger(0);
    m.time      = QDateTime::currentDateTime();
    return m;
}

// Returns a copy of `body` with secret fields replaced by "***".
// Used before a message is logged or saved to a Connection's history, so
// passwords and pins never end up in the log file or in memory history.
// If the protocol gets a new secret field, add its name to this list.
inline QJsonObject redactSecrets(QJsonObject body)
{
    static const QStringList secretFields = { "password", "newPassword", "pin" };
    for (const QString& field : secretFields)
        if (body.contains(field))
            body[field] = "***";
    return body;
}
