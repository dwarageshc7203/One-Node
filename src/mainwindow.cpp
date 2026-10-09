#include "mainwindow.h"
#ifdef Q_OS_WIN
#include "udpbeaconbackend.h"
#else
#include "mdnsadvertiser.h"
#endif
#include <QApplication>
#include <QIcon>
#include <QPixmap>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QRandomGenerator>
#include <QFont>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QMimeData>
#include <QHostAddress>
#include <QUrl>
#include <QDebug>
#include <QMessageAuthenticationCode>
#include <QCryptographicHash>

namespace {
qint32 readBigEndianInt32(const QByteArray &bytes)
{
    return (static_cast<quint8>(bytes[0]) << 24)
         | (static_cast<quint8>(bytes[1]) << 16)
         | (static_cast<quint8>(bytes[2]) << 8)
         | static_cast<quint8>(bytes[3]);
}
qint64 readBigEndianInt64(const QByteArray &bytes)
{
    qint64 value = 0;
    for (int i = 0; i < 8; ++i) {
        value = (value << 8) | static_cast<quint8>(bytes[i]);
    }
    return value;
}
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    settings       = new QSettings("One Node", "One Node", this);
    countdownTimer = new QTimer(this);
    pairingServer  = new PairingServer(this);
    fileTransfer   = new FileTransfer(this);
#ifdef Q_OS_WIN
    discoveryBackend = new UdpBeaconBackend(this);
#else
    discoveryBackend = new MdnsAdvertiser(this);
#endif
    heartbeatTimer = new QTimer(this);
    desktopReceiverServer = nullptr;
    pingFailures = 0;
    heartbeatOnline = false;
    pingInFlight = false;

    connect(countdownTimer, &QTimer::timeout,
            this, &MainWindow::onTickTimer);
    connect(pairingServer, &PairingServer::devicePaired,
            this, &MainWindow::onDevicePaired);
    connect(pairingServer, &PairingServer::pairingFailed,
            this, &MainWindow::onPairingFailed);
    connect(fileTransfer, &FileTransfer::transferDone,
            this, &MainWindow::onTransferDone);
    connect(fileTransfer, &FileTransfer::transferFailed,
            this, &MainWindow::onTransferFailed);
    connect(fileTransfer, &FileTransfer::transferStarted,
            this, [this](const QString &fileName) {
                progressBar->setValue(0);
                progressBar->show();
            });
    connect(fileTransfer, &FileTransfer::transferProgress,
            this, [this](int percent) {
                progressBar->setValue(percent);
            });
    connect(heartbeatTimer, &QTimer::timeout,
            this, &MainWindow::sendHeartbeatPing);
    heartbeatTimer->setInterval(3000);

    setupUI();
    setupTray();
    setupDesktopReceiver();
    discoveryBackend->start();
    setAcceptDrops(true);

    const QString savedIp = settings->value("device_ip").toString();
    const QString savedToken = settings->value("pairing_secret").toString();
    if (!savedIp.isEmpty() && !savedToken.isEmpty()) {
        showLinkedState(settings->value("device_name").toString());
    } else {
        onRegenerateClicked();
    }
}

// ── Drag and drop ──────────────────────────────────────────────────────────

void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
    qDebug() << "dragEnterEvent triggered";
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        qDebug() << "Drag accepted";
    }
}

void MainWindow::dropEvent(QDropEvent *event) {
    qDebug() << "dropEvent triggered";

    QString peerIp = settings->value("device_ip").toString();
    QString deviceName = settings->value("device_name").toString();

    if (peerIp.isEmpty() || deviceName.isEmpty()) {
        statusLabel->setText("⚠️  No device linked — pair first");
        return;
    }

    const auto urls = event->mimeData()->urls();
    QString token = settings->value("pairing_secret").toString();
    for (const QUrl &url : urls) {
        QString filePath = url.toLocalFile();
        if (!filePath.isEmpty()) {
            statusLabel->setText("📤  Sending " + QFileInfo(filePath).fileName());
            fileTransfer->sendFile(filePath, peerIp, 45679, token);
        }
    }
}

// ── File sending ───────────────────────────────────────────────────────────

void MainWindow::sendFilePath(const QString &filePath) {
    QString peerIp = settings->value("device_ip").toString();
    if (peerIp.isEmpty()) {
        trayIcon->showMessage("One Node", "No device linked — open app to pair",
                              QSystemTrayIcon::Warning, 3000);
        return;
    }
    trayIcon->showMessage("One Node",
        "Sending: " + QFileInfo(filePath).fileName(),
        QSystemTrayIcon::Information, 2000);
    QString token = settings->value("pairing_secret").toString();
    fileTransfer->sendFile(filePath, peerIp, 45679, token);
}

// ── Pairing ────────────────────────────────────────────────────────────────

QString MainWindow::generateCode() {
    int n = QRandomGenerator::global()->bounded(100000, 999999);
    return QString::number(n);
}

void MainWindow::applyCode(const QString &code) {
    currentCode = code;
    QString display = code.left(3) + "  " + code.right(3);
    codeLabel->setText(display);
    statusLabel->setText("⏳  Waiting for device...");
    secondsLeft = 300;
    countdownTimer->start(1000);
    onTickTimer();
    pairingServer->start(code);
}

void MainWindow::onTickTimer() {
    if (secondsLeft <= 0) {
        timerLabel->setText("Code expired — click Regenerate");
        countdownTimer->stop();
        pairingServer->stop();
        return;
    }
    int m = secondsLeft / 60;
    int s = secondsLeft % 60;
    timerLabel->setText(QString("Expires in %1:%2")
        .arg(m).arg(s, 2, 10, QChar('0')));
    secondsLeft--;
}

void MainWindow::onRegenerateClicked() {
    stopHeartbeat();
    linkedLabel->clear();
    trayIcon->setToolTip("One Node — not linked");
    applyCode(generateCode());
}

void MainWindow::onDevicePaired(const QString &deviceName, const QString &deviceId, const QString &secret, const QString &serverDeviceId, const QString &deviceIp) {
    countdownTimer->stop();
    QString cleanIp = deviceIp;
    if (cleanIp.startsWith("::ffff:"))
        cleanIp = cleanIp.mid(7);
    settings->setValue("device_name", deviceName);
    settings->setValue("device_id", deviceId);
    settings->setValue("server_device_id", serverDeviceId);
    settings->setValue("pairing_secret", secret);
    settings->setValue("device_ip", cleanIp);
    settings->sync();
    showLinkedState(deviceName);
}

void MainWindow::onPairingFailed(const QString &reason) {
    statusLabel->setText("❌  " + reason);
}

void MainWindow::showLinkedState(const QString &deviceName) {
    codeLabel->setText("Linked ✓");
    timerLabel->setText("");
    statusLabel->setText("✅  Connected to " + deviceName);
    linkedLabel->setText("Linked to: " + deviceName);
    regenerateBtn->setText("Unlink");
    trayIcon->setToolTip("One Node — linked to " + deviceName);
    startHeartbeat();

    disconnect(regenerateBtn, &QPushButton::clicked, nullptr, nullptr);
    connect(regenerateBtn, &QPushButton::clicked, this, [this]() {
        stopHeartbeat();
        settings->remove("device_name");
        settings->remove("device_id");
        settings->remove("server_device_id");
        settings->remove("pairing_secret");
        settings->remove("device_ip");
        settings->sync();
        regenerateBtn->setText("Regenerate");
        linkedLabel->clear();
        trayIcon->setToolTip("One Node — not linked");
        disconnect(regenerateBtn, nullptr, this, nullptr);
        connect(regenerateBtn, &QPushButton::clicked,
                this, &MainWindow::onRegenerateClicked);
        onRegenerateClicked();
    });
}

void MainWindow::onTransferDone(const QString &fileName) {
    statusLabel->setText("✅  Sent: " + fileName);
    trayIcon->showMessage("One Node", "Sent: " + fileName,
                          QSystemTrayIcon::Information, 3000);
    progressBar->hide();
}

void MainWindow::onTransferFailed(const QString &reason) {
    statusLabel->setText("❌  Failed: " + reason);
    trayIcon->showMessage("One Node", "Transfer failed — device may be offline",
                          QSystemTrayIcon::Warning, 3000);
    progressBar->hide();

    if (reason.startsWith("Connection error:", Qt::CaseInsensitive)
        || reason.contains("refused", Qt::CaseInsensitive)
        || reason.contains("timed out", Qt::CaseInsensitive)
        || reason.contains("network is unreachable", Qt::CaseInsensitive)
        || reason.contains("host not found", Qt::CaseInsensitive)) {
        
        heartbeatOnline = false;
        statusLabel->setText("⚠️  Device offline");
    }
}

// ── UI Setup ───────────────────────────────────────────────────────────────

void MainWindow::setupUI() {
    setWindowTitle("One Node — Link device");
    resize(460, 520);
    setMinimumSize(420, 460);
    setWindowIcon(QIcon(":/icon.png"));

    QWidget *central = new QWidget(this);
    setCentralWidget(central);

    QVBoxLayout *root = new QVBoxLayout(central);
    root->setContentsMargins(24, 24, 24, 24);
    root->setSpacing(16);
    root->setSizeConstraint(QLayout::SetMinimumSize);

    QLabel *title = new QLabel("Link your phone", this);
    QFont tf = title->font();
    tf.setPointSize(15);
    tf.setWeight(QFont::Medium);
    title->setFont(tf);
    title->setAlignment(Qt::AlignCenter);

    QLabel *subtitle = new QLabel("Open One Node on Android and enter this code", this);
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setStyleSheet("color: gray; font-size: 12px;");

    root->addWidget(title);
    root->addWidget(subtitle);

    QFrame *card = new QFrame(this);
    card->setFrameShape(QFrame::StyledPanel);
    card->setStyleSheet(
        "QFrame { border: 1px solid #ddd; border-radius: 12px; background: white; }"
    );

    QVBoxLayout *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(20, 20, 20, 20);
    cardLayout->setSpacing(12);

    QLabel *codeHint = new QLabel("Your pairing code", card);
    codeHint->setStyleSheet("color: gray; font-size: 11px; border: none;");

    codeLabel = new QLabel("--- ---", card);
    QFont cf = codeLabel->font();
    cf.setPointSize(28);
    cf.setWeight(QFont::Medium);
    cf.setFamily("monospace");
    codeLabel->setFont(cf);
    codeLabel->setAlignment(Qt::AlignCenter);
    codeLabel->setStyleSheet("letter-spacing: 3px; border: none; color: #1a1a1a;");

    timerLabel = new QLabel("", card);
    timerLabel->setAlignment(Qt::AlignCenter);
    timerLabel->setStyleSheet("color: gray; font-size: 12px; border: none;");

    QHBoxLayout *bottomRow = new QHBoxLayout();
    statusLabel = new QLabel("⏳  Waiting...", card);
    statusLabel->setStyleSheet("color: gray; font-size: 12px; border: none;");

    regenerateBtn = new QPushButton("Regenerate", card);
    regenerateBtn->setStyleSheet(
        "QPushButton { border: 1px solid #534AB7; border-radius: 6px;"
        "padding: 5px 14px; font-size: 12px; background: white; color: #534AB7; font-weight: bold; }"
        "QPushButton:hover { background: #EEEDFE; }"
    );
    connect(regenerateBtn, &QPushButton::clicked,
            this, &MainWindow::onRegenerateClicked);

    bottomRow->addWidget(statusLabel);
    bottomRow->addStretch();
    bottomRow->addWidget(regenerateBtn);

    cardLayout->addWidget(codeHint);
    cardLayout->addWidget(codeLabel);
    cardLayout->addWidget(timerLabel);
    cardLayout->addSpacing(4);
    cardLayout->addLayout(bottomRow);

    root->addWidget(card);

    linkedLabel = new QLabel("", this);
    linkedLabel->setAlignment(Qt::AlignCenter);
    linkedLabel->setStyleSheet("color: gray; font-size: 12px;");
    root->addWidget(linkedLabel);

    progressBar = new QProgressBar(this);
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    progressBar->hide();
    root->addWidget(progressBar);

    // Drop zone
    QLabel *hint = new QLabel("🗂  Drop any file here to send it to your phone.", this);
    hint->setAlignment(Qt::AlignCenter);
    hint->setStyleSheet(
        "background: #EEEDFE; border-radius: 8px;"
        "padding: 16px; color: #534AB7; font-size: 13px;"
        "border: 2px dashed #AFA9EC;"
    );
    hint->setWordWrap(true);
    hint->setMinimumHeight(60);
    root->addWidget(hint);
    root->addStretch();
}

// ── Tray ───────────────────────────────────────────────────────────────────

void MainWindow::setupTray() {
    trayMenu = new QMenu(this);
    trayMenu->addAction("Open", this, &MainWindow::show);
    trayMenu->addSeparator();
    trayMenu->addAction("Quit", this, &MainWindow::onQuitClicked);

    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setIcon(QIcon(":/icon.png"));
    trayIcon->setContextMenu(trayMenu);
    trayIcon->setToolTip("One Node — not linked");
    trayIcon->show();

    connect(trayIcon, &QSystemTrayIcon::activated,
            this, &MainWindow::onTrayActivated);
}

void MainWindow::onTrayActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::Trigger)
        isVisible() ? hide() : show();
}

void MainWindow::onQuitClicked() {
    QApplication::quit();
}

// ── Desktop receiver (phone → desktop) ────────────────────────────────────

void MainWindow::setupDesktopReceiver() {
    desktopReceiverServer = new QTcpServer(this);
    connect(desktopReceiverServer, &QTcpServer::newConnection, this, [this]() {
        while (desktopReceiverServer->hasPendingConnections()) {
            QTcpSocket *socket = desktopReceiverServer->nextPendingConnection();
            
            IncomingTransferState state;
            state.serverNonce.resize(32);
            for (int i = 0; i < 32; ++i) {
                state.serverNonce[i] = static_cast<char>(QRandomGenerator::system()->bounded(256));
            }
            
            QByteArray greeting;
            greeting.append(0x01); // version
            greeting.append(state.serverNonce);
            socket->write(greeting);
            socket->flush();
            
            incomingTransfers.insert(socket, state);
            
            connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
                processIncomingTransfer(socket);
            });
            connect(socket, &QTcpSocket::disconnected, this, [this, socket]() {
                cleanupIncomingTransfer(socket);
            });
        }
    });
    if (!desktopReceiverServer->listen(QHostAddress::Any, 45680)) {
        qWarning() << "Failed to start incoming file server:" << desktopReceiverServer->errorString();
    }
}

void MainWindow::processIncomingTransfer(QTcpSocket *socket) {
    if (!socket || !socket->isValid()) return;

    auto it = incomingTransfers.find(socket);
    if (it == incomingTransfers.end()) return;

    IncomingTransferState &state = it.value();
    state.buffer.append(socket->readAll());

    if (!state.authenticated) {
        if (state.hState == IncomingTransferState::WaitClientGreeting) {
            // Wait for 32 (clientNonce) + 32 (hmacC) + 2 (idLen) = 66 bytes min
            if (state.buffer.size() < 66) return;

            QByteArray clientNonce = state.buffer.left(32);
            QByteArray hmacC = state.buffer.mid(32, 32);
            quint16 idLen = (static_cast<quint8>(state.buffer[64]) << 8) | static_cast<quint8>(state.buffer[65]);

            if (state.buffer.size() < 66 + idLen) return;

            QByteArray recvId = state.buffer.mid(66, idLen);
            state.buffer.remove(0, 66 + idLen);

            QByteArray secret = QByteArray::fromBase64(settings->value("pairing_secret").toString().toUtf8());
            QByteArray myId = settings->value("server_device_id").toString().toUtf8();
            // In a multi-device setup, we would look up secret by recvId here. 
            // For now, we only support one paired device in QSettings.
            QByteArray peerId = settings->value("device_id").toString().toUtf8();

            if (recvId != peerId) {
                qWarning() << "Unknown peer ID!";
                cleanupIncomingTransfer(socket);
                socket->disconnectFromHost();
                return;
            }

            QByteArray msgC = "ONv1-C" + state.serverNonce + recvId + QByteArray("\x00", 1) + myId;
            QMessageAuthenticationCode macC(QCryptographicHash::Sha256);
            macC.setKey(secret);
            macC.addData(msgC);
            QByteArray expectedHmacC = macC.result();

            int diff = 0;
            for (int i = 0; i < 32; ++i) diff |= (hmacC[i] ^ expectedHmacC[i]);
            if (diff != 0) {
                cleanupIncomingTransfer(socket);
                socket->disconnectFromHost();
                return;
            }

            // Send HMAC_S
            QByteArray msgS = "ONv1-S" + clientNonce + myId + QByteArray("\x00", 1) + recvId;
            QMessageAuthenticationCode macS(QCryptographicHash::Sha256);
            macS.setKey(secret);
            macS.addData(msgS);
            QByteArray hmacS = macS.result();
            socket->write(hmacS);
            socket->flush();

            state.hState = IncomingTransferState::Completed;
            state.authenticated = true;
        }
    }

    if (!state.authenticated) return;

    while (true) {
        if (state.nameLength < 0) {
            if (state.buffer.size() < 4) return;
            state.nameLength = readBigEndianInt32(state.buffer.left(4));
            state.buffer.remove(0, 4);

            if (state.nameLength <= 0 || state.nameLength > 4096) {
                qWarning() << "Invalid file name length:" << state.nameLength;
                cleanupIncomingTransfer(socket);
                socket->disconnectFromHost();
                return;
            }
        }
        if (state.fileName.isEmpty()) {
            if (state.buffer.size() < state.nameLength) return;
            state.fileName = QString::fromUtf8(state.buffer.left(state.nameLength));
            state.fileName = QFileInfo(state.fileName).fileName();
            state.buffer.remove(0, state.nameLength);
        }
        if (state.fileSize < 0) {
            if (state.buffer.size() < 8) return;
            state.fileSize = readBigEndianInt64(state.buffer.left(8));
            state.buffer.remove(0, 8);

            if (state.fileSize < 0) {
                qWarning() << "Invalid incoming file size:" << state.fileSize;
                cleanupIncomingTransfer(socket);
                socket->disconnectFromHost();
                return;
            }

            QString downloadsPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
            if (downloadsPath.isEmpty()) downloadsPath = QDir::homePath();

            QDir targetDir(downloadsPath + "/OneNode");
            targetDir.mkpath(".");

            QString safeName = state.fileName.isEmpty() ? "shared_file.bin" : state.fileName;
            state.outputFile = new QFile(targetDir.filePath(safeName), socket);
            if (!state.outputFile->open(QIODevice::WriteOnly)) {
                cleanupIncomingTransfer(socket);
                socket->disconnectFromHost();
                return;
            }
            trayIcon->showMessage("One Node", "Receiving " + safeName,
                                  QSystemTrayIcon::Information, 2000);
        }
        if (!state.outputFile) return;

        qint64 remaining = state.fileSize - state.received;
        if (remaining <= 0) {
            state.outputFile->close();
            trayIcon->showMessage("One Node", "Received " + state.fileName,
                                  QSystemTrayIcon::Information, 3000);
            progressBar->hide();
            cleanupIncomingTransfer(socket);
            socket->disconnectFromHost();
            return;
        }

        qint64 toWrite = qMin<qint64>(remaining, state.buffer.size());
        if (toWrite == 0) return;

        state.outputFile->write(state.buffer.constData(), toWrite);
        state.buffer.remove(0, static_cast<int>(toWrite));
        state.received += toWrite;
        
        int percent = (int)((state.received * 100) / state.fileSize);
        progressBar->setValue(percent);
        progressBar->show();

        if (state.received >= state.fileSize) {
            state.outputFile->flush();
            state.outputFile->close();
            trayIcon->showMessage("One Node", "Received " + state.fileName,
                                  QSystemTrayIcon::Information, 3000);
            progressBar->hide();
            cleanupIncomingTransfer(socket);
            socket->disconnectFromHost();
            return;
        }
    }
}

void MainWindow::cleanupIncomingTransfer(QTcpSocket *socket) {
    auto it = incomingTransfers.find(socket);
    if (it == incomingTransfers.end()) return;
    if (it.value().outputFile) {
        it.value().outputFile->close();
        it.value().outputFile->deleteLater();
        it.value().outputFile = nullptr;
    }
    incomingTransfers.erase(it);
    socket->deleteLater();
}

void MainWindow::startHeartbeat() {
    pingFailures = 0;
    heartbeatOnline = true;
    pingInFlight = false;
    heartbeatTimer->start();
}

void MainWindow::stopHeartbeat() {
    heartbeatTimer->stop();
    pingInFlight = false;
}

void MainWindow::sendHeartbeatPing() {
    if (pingInFlight) return;

    QString peerIp = settings->value("device_ip").toString();
    QString token  = settings->value("pairing_secret").toString();

    if (peerIp.isEmpty() || token.isEmpty()) return;

    pingInFlight = true;

    QTcpSocket *socket = new QTcpSocket(this);

    QTimer::singleShot(2500, socket, [this, socket]() {
        if (socket->state() == QAbstractSocket::ConnectedState
            || socket->state() == QAbstractSocket::ConnectingState) {
            markHeartbeatFailure();
            socket->abort();
            socket->deleteLater();
            pingInFlight = false;
        }
    });

    connect(socket, &QTcpSocket::connected, this, [this, socket]() {
        socket->write("ping\n");
        socket->flush();
    });

    connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
        QByteArray response = socket->readAll().trimmed();
        if (response == "pong") {
            markHeartbeatSuccess();
        } else {
            markHeartbeatFailure();
        }
        socket->disconnectFromHost();
        socket->deleteLater();
        pingInFlight = false;
    });

    connect(socket, &QTcpSocket::errorOccurred, this, [this, socket]() {
        markHeartbeatFailure();
        socket->deleteLater();
        pingInFlight = false;
    });

    socket->connectToHost(peerIp, 45681);
}

void MainWindow::markHeartbeatSuccess() {
    pingFailures = 0;

    if (!heartbeatOnline) {
        heartbeatOnline = true;
        statusLabel->setText("✅  Connection restored");
    }
}

void MainWindow::markHeartbeatFailure() {
    pingFailures++;

    if (pingFailures >= 3) {
        heartbeatTimer->stop();
        heartbeatOnline = false;
        statusLabel->setText("⚠️  Device may be offline");
    }
}