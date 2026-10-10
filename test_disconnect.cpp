#include <QCoreApplication>
#include <QTcpSocket>
#include <QDebug>
#include <QTimer>

int main(int argc, char *argv[]) {
    QCoreApplication a(argc, argv);
    QTcpSocket socket;
    QObject::connect(&socket, &QTcpSocket::errorOccurred, [](QAbstractSocket::SocketError err){
        qDebug() << "ERROR OCCURRED:" << err;
    });
    QObject::connect(&socket, &QTcpSocket::disconnected, [](){
        qDebug() << "DISCONNECTED";
        qApp->quit();
    });
    QObject::connect(&socket, &QTcpSocket::connected, [&](){
        qDebug() << "CONNECTED";
        socket.disconnectFromHost();
    });
    socket.connectToHost("google.com", 80);
    return a.exec();
}
