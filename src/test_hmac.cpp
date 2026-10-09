#include <QtTest>
#include <QMessageAuthenticationCode>
#include <QCryptographicHash>
#include <QByteArray>

class HmacTest : public QObject {
    Q_OBJECT

private slots:
    void testHmacVectors() {
        QByteArray secret(32, 0x01);
        QByteArray serverNonce(32, 0x02);
        QByteArray clientNonce(32, 0x03);
        QByteArray serverId = "ServerID";
        QByteArray clientId = "ClientID";

        QByteArray expectedHmacCHex = "aecb443a2e94a4c9b9e784f4f8221f5656aca36f3ceaa18df7c4a7a399db4b80";
        QByteArray expectedHmacSHex = "4563e5b38978731bf5103043c6a9191175c355a1a3842a5ee4cff74f42d0ce8c";

        QByteArray msgC = "ONv1-C" + serverNonce + clientId + QByteArray("\x00", 1) + serverId;
        QMessageAuthenticationCode macC(QCryptographicHash::Sha256);
        macC.setKey(secret);
        macC.addData(msgC);
        QByteArray hmacC = macC.result();

        QByteArray msgS = "ONv1-S" + clientNonce + serverId + QByteArray("\x00", 1) + clientId;
        QMessageAuthenticationCode macS(QCryptographicHash::Sha256);
        macS.setKey(secret);
        macS.addData(msgS);
        QByteArray hmacS = macS.result();

        QCOMPARE(hmacC.toHex(), expectedHmacCHex);
        QCOMPARE(hmacS.toHex(), expectedHmacSHex);
    }
};

QTEST_MAIN(HmacTest)
#include "test_hmac.moc"
