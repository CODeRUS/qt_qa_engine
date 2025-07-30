#pragma once
#include <qt_qa_engine/ITransportServer.h>

#include <QHash>
#include <QObject>

class QTcpServer;
class TCPSocketServer : public ITransportServer
{
    Q_OBJECT
public:
    explicit TCPSocketServer(quint16 port = 8888, QObject* parent = nullptr);

public slots:
    void start();

private slots:
    void newConnection();

private:
    quint16 m_port = 8888;
    QTcpServer* m_server = nullptr;
};
