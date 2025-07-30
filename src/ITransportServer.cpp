#include <qt_qa_engine/ITransportClient.h>
#include <qt_qa_engine/ITransportServer.h>

#include <QDebug>
#include <QJsonDocument>
#include <QJsonParseError>

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(categoryITransportServer, "autoqa.qaengine.transport.interface", QtWarningMsg)

ITransportServer::ITransportServer(QObject* parent)
    : QObject(parent)
{
}

void ITransportServer::registerClient(ITransportClient* client)
{
    connect(client, &ITransportClient::readyRead, this, &ITransportServer::readData);
    connect(client, &ITransportClient::disconnected, this, &ITransportServer::clientLost);
}

void ITransportServer::readData(ITransportClient* client)
{
    auto bytes = client->bytesAvailable();
    qCDebug(categoryITransportServer) << Q_FUNC_INFO << client << bytes;

    static QByteArray requestData;
    requestData.append(client->readAll());
    qCDebug(categoryITransportServer).noquote() << requestData;

    requestData.replace("}{", "}\n{"); // workaround packets join

    const QList<QByteArray> commands = requestData.split('\n');
    qCDebug(categoryITransportServer) << Q_FUNC_INFO << "Commands count:" << commands.length();

    requestData.clear();
    for (const QByteArray& cmd : commands)
    {
        if (cmd.isEmpty())
        {
            continue;
        }
        qCDebug(categoryITransportServer) << Q_FUNC_INFO << "Command:";
        qCDebug(categoryITransportServer).noquote() << cmd;

        QJsonParseError error;
        QJsonDocument::fromJson(cmd, &error);
        if (error.error != QJsonParseError::NoError)
        {
            qCDebug(categoryITransportServer) << Q_FUNC_INFO << "Partial data, waiting for more";
            qCDebug(categoryITransportServer).noquote() << error.errorString();

            requestData = cmd;
            break;
        }

        emit commandReceived(client, cmd);
    }
}
