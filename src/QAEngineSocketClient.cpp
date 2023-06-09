// Copyright (c) 2019-2020 Open Mobile Platform LLC.
#include <qt_qa_engine/QAEngine.h>
#include <qt_qa_engine/QAEngineSocketClient.h>
#include <qt_qa_engine/TCPSocketClient.h>

#include <QDebug>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLocalSocket>
#include <QTcpSocket>

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(categorySocketClient, "autoqa.qaengine.socket", QtWarningMsg)

QAEngineSocketClient::QAEngineSocketClient(QObject* parent)
    : QObject(parent)
{
}

void QAEngineSocketClient::connectToBridge()
{
    qCDebug(categorySocketClient) << Q_FUNC_INFO;

    auto socket = new QTcpSocket(this);
    socket->connectToHost(QHostAddress(QHostAddress::LocalHost), 8888);
    m_client = new TCPSocketClient(socket, this);

    if (!m_client->isConnected())
    {
        qCDebug(categorySocketClient) << Q_FUNC_INFO << m_client << "is not connected!";
        return;
    }

    QJsonObject root;
    QJsonObject app;
    app.insert(QStringLiteral("appName"), QAEngine::processName());

    root.insert(QStringLiteral("appConnect"), app);

    QByteArray data = QJsonDocument(root).toJson(QJsonDocument::Compact);
    auto bytes = m_client->write(data);
    qCDebug(categorySocketClient) << Q_FUNC_INFO << "Bytes to write:" << bytes;
    auto success = m_client->waitForBytesWritten();
    qCDebug(categorySocketClient) << "Writing to bridge socket:" << success;

    connect(m_client,
            &ITransportClient::readyRead,
            this,
            &QAEngineSocketClient::readClient,
            Qt::UniqueConnection);
}

void QAEngineSocketClient::readClient(ITransportClient* client)
{
    auto bytes = client->bytesAvailable();
    qCDebug(categorySocketClient) << Q_FUNC_INFO << client << bytes;

    emit commandReceived(client, client->readAll());
}

void QAEngineSocketClient::onConnected()
{
    qCDebug(categorySocketClient) << Q_FUNC_INFO << Qt::endl
                                  << "client:" << m_client << Qt::endl
                                  << "is connected:" << m_client->isConnected();

    QJsonObject root;
    QJsonObject app;
    app.insert(QStringLiteral("appName"), QAEngine::processName());

    root.insert(QStringLiteral("appConnect"), app);

    QByteArray data = QJsonDocument(root).toJson(QJsonDocument::Compact);
    auto bytes = m_client->write(data);
    qCDebug(categorySocketClient) << Q_FUNC_INFO << "Bytes to write:" << bytes;
    auto success = m_client->waitForBytesWritten();
    qCDebug(categorySocketClient) << "Writing to bridge socket:" << success;

    qCDebug(categorySocketClient) << Q_FUNC_INFO << Qt::endl
                                  << "ready read:" << m_client->waitForReadyRead() << Qt::endl;

    auto available = m_client->bytesAvailable();
    qCDebug(categorySocketClient) << Q_FUNC_INFO << m_client << available;

    qCDebug(categorySocketClient).noquote() << m_client->readAll();
}
