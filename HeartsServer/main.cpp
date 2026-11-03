// Minimal Qt TCP test server: listens on all IPv4 interfaces,
// logs connects/disconnects, prints each line a client sends,
// and replies "ack: <line>" so the client can confirm the round trip.
//
// Usage: test_server [port]   (default 5000)

#include <QCoreApplication>
#include <QDateTime>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTextStream>

static QTextStream out(stdout);

static void log(const QString& msg)
{
    out << QDateTime::currentDateTime().toString("hh:mm:ss") << "  " << msg << Qt::endl;
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    quint16 port = 5000;
    if (argc > 1) {
        bool ok = false;
        port = QString(argv[1]).toUShort(&ok);
        if (!ok || port == 0) {
            log("invalid port: " + QString(argv[1]));
            return 1;
        }
    }

    QTcpServer server;

    QObject::connect(&server, &QTcpServer::newConnection, [&server] {
        while (server.hasPendingConnections()) {
            QTcpSocket* sock = server.nextPendingConnection();
            const QString who = sock->peerAddress().toString() + ":" +
                                QString::number(sock->peerPort());
            log("connected     " + who);

            QObject::connect(sock, &QTcpSocket::readyRead, [sock, who] {
                while (sock->canReadLine()) {
                    const QByteArray line = sock->readLine().trimmed();
                    log(who + "  > " + QString::fromUtf8(line));
                    sock->write("ack: " + line + "\n");
                }
            });

            QObject::connect(sock, &QTcpSocket::disconnected, [sock, who] {
                log("disconnected  " + who);
                sock->deleteLater();
            });
        }
    });

    // AnyIPv4 = 0.0.0.0, so other machines on the network can reach it.
    if (!server.listen(QHostAddress::AnyIPv4, port)) {
        log("listen failed: " + server.errorString());
        return 1;
    }

    log("listening on port " + QString::number(port) + " at:");
    for (const QHostAddress& addr : QNetworkInterface::allAddresses()) {
        if (addr.protocol() == QAbstractSocket::IPv4Protocol && !addr.isLoopback())
            log("  " + addr.toString());
    }

    return app.exec();
}