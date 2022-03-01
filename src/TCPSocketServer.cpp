// Copyright (c) 2019-2020 Open Mobile Platform LLC.
#include <qt_qa_engine/TCPSocketClient.h>
#include <qt_qa_engine/TCPSocketServer.h>

#include <QCoreApplication>
#include <QTcpServer>
#include <QTcpSocket>

#include <QDebug>

TCPSocketServer::TCPSocketServer(quint16 port, QObject* parent)
    : ITransportServer(parent)
    , m_port(port)
    , m_server(new QTcpServer(this))
{
    connect(m_server, &QTcpServer::newConnection, this, &TCPSocketServer::newConnection);
}

void TCPSocketServer::start()
{
    qDebug() << Q_FUNC_INFO;

    if (!m_server->listen(QHostAddress::AnyIPv4, m_port))
    {
        qWarning() << Q_FUNC_INFO << m_server->errorString();
        qApp->quit();
        return;
    }
}

void TCPSocketServer::newConnection()
{
    auto socket = m_server->nextPendingConnection();
    qDebug() << Q_FUNC_INFO << "New connection from:" << socket->peerAddress()
             << socket->peerPort();
    auto client = new TCPSocketClient(socket);
    registerClient(client);
}
