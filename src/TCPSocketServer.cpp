#include <qt_qa_engine/TCPSocketClient.h>
#include <qt_qa_engine/TCPSocketServer.h>

#include <QCoreApplication>
#include <QTcpServer>
#include <QTcpSocket>

#include <QDebug>

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(categoryTCPSocketServer, "autoqa.qaengine.transport.server", QtWarningMsg)

TCPSocketServer::TCPSocketServer(quint16 port, QObject* parent)
    : ITransportServer(parent)
    , m_port(port)
    , m_server(new QTcpServer(this))
{
    connect(m_server, &QTcpServer::newConnection, this, &TCPSocketServer::newConnection);
}

void TCPSocketServer::start()
{
    qCDebug(categoryTCPSocketServer) << Q_FUNC_INFO;
    if (m_server->isListening())
    {
        return;
    }

    for (int i = 0; i < 10; i++) {
        if (m_server->listen(QHostAddress::AnyIPv4, m_port + i)) {
            m_port = m_port + i;
            break;
        } else {
            qCWarning(categoryTCPSocketServer) << Q_FUNC_INFO << m_server->errorString();
        }
    }

    if (!m_server->isListening())
    {
        qCWarning(categoryTCPSocketServer) << Q_FUNC_INFO << m_server->errorString();
        qApp->quit();
        return;
    }
    else
    {
        qCWarning(categoryTCPSocketServer) << Q_FUNC_INFO << "listening:" << m_port;
    }
}

void TCPSocketServer::newConnection()
{
    auto socket = m_server->nextPendingConnection();
    qCDebug(categoryTCPSocketServer)
        << Q_FUNC_INFO << "New connection from:" << socket->peerAddress() << socket->peerPort();
    auto client = new TCPSocketClient(socket);
    registerClient(client);
}
