#include "tcpclient.h"

#include <QApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMessageBox>
#include <QTimer>

namespace {
// 本机唯一管道名，勿与其它应用冲突；用于单实例与二次启动唤醒
const char kSingleInstanceServerName[] = "AGTTcpClient_SingleInstance_8756B2A1";

bool tryActivateRunningInstance()
{
    QLocalSocket socket;
    socket.connectToServer(QString::fromLatin1(kSingleInstanceServerName));
    if (!socket.waitForConnected(800))
        return false;
    socket.write("show");
    socket.flush();
    socket.waitForBytesWritten(300);
    return true;
}
} // namespace

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    if (tryActivateRunningInstance())
        return 0;

    QLocalServer server;
    if (!server.listen(QString::fromLatin1(kSingleInstanceServerName))) {
        // 与另一启动几乎同时竞争：对端已 listen，则连上并退出
        if (tryActivateRunningInstance())
            return 0;
        QLocalServer::removeServer(QString::fromLatin1(kSingleInstanceServerName));
        if (!server.listen(QString::fromLatin1(kSingleInstanceServerName))) {
            if (tryActivateRunningInstance())
                return 0;
            QMessageBox::critical(nullptr, QObject::tr("启动失败"),
                                  QObject::tr("无法创建单实例锁：%1").arg(server.errorString()));
            return 1;
        }
    }

    tcpClient w;

    QObject::connect(&server, &QLocalServer::newConnection, &w, [&server, &w]() {
        QLocalSocket *client = server.nextPendingConnection();
        if (client) {
            QObject::connect(client, &QLocalSocket::disconnected, client, &QLocalSocket::deleteLater);
            client->deleteLater();
        }
        // 延后到事件循环，便于在 Windows 上正确置前、去最小化
        QTimer::singleShot(0, &w, [&w]() {
            if (w.isMinimized())
                w.showNormal();
            w.raise();
            w.activateWindow();
        });
    });

    w.show();
    return a.exec();
}
