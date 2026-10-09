#pragma once
#include <QMainWindow>
#include <QHash>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QSettings>
#include <QProgressBar>
#include <QTcpServer>
#include <QTcpSocket>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include "pairingserver.h"
#include "filetransfer.h"

class QFile;
class DiscoveryBackend;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    void sendFilePath(const QString &filePath);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void onQuitClicked();
    void onRegenerateClicked();
    void onTickTimer();
    void onDevicePaired(const QString &deviceName, const QString &deviceId, const QString &secret, const QString &serverDeviceId, const QString &deviceIp);
    void onPairingFailed(const QString &reason);
    void onTransferDone(const QString &fileName);
    void onTransferFailed(const QString &reason);

private:
    struct IncomingTransferState {
        enum HandshakeState { WaitClientGreeting, Completed };
        HandshakeState hState = WaitClientGreeting;
        QByteArray hBuffer;
        QByteArray serverNonce;
        bool authenticated = false;
        
        qint32 nameLength = -1;
        QString fileName;
        qint64 fileSize = -1;
        qint64 received = 0;
        QByteArray buffer;
        QFile *outputFile = nullptr;
    };

    void setupUI();
    void setupTray();
    void setupDesktopReceiver();
    QString generateCode();
    void applyCode(const QString &code);
    void showLinkedState(const QString &deviceName);
    void startHeartbeat();
    void stopHeartbeat();
    void sendHeartbeatPing();
    void markHeartbeatSuccess();
    void markHeartbeatFailure();
    void processIncomingTransfer(QTcpSocket *socket);
    void cleanupIncomingTransfer(QTcpSocket *socket);

    QSystemTrayIcon *trayIcon;
    QMenu           *trayMenu;
    QTcpServer      *desktopReceiverServer;

    QLabel      *codeLabel;
    QLabel      *statusLabel;
    QLabel      *timerLabel;
    QLabel      *linkedLabel;
    QPushButton *regenerateBtn;
    QProgressBar *progressBar;

    QSettings      *settings;
    QTimer         *countdownTimer;
    PairingServer  *pairingServer;
    FileTransfer   *fileTransfer;
    DiscoveryBackend *discoveryBackend;
    QTimer         *heartbeatTimer;
    int             pingFailures;
    bool            heartbeatOnline;
    bool            pingInFlight;
    QHash<QTcpSocket *, IncomingTransferState> incomingTransfers;
    int             secondsLeft;
    QString         currentCode;
};