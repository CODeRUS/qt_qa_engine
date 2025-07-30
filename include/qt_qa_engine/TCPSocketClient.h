#pragma once
#include <qt_qa_engine/ITransportClient.h>

class QTcpSocket;
class TCPSocketClient : public ITransportClient
{
    Q_OBJECT
public:
    explicit TCPSocketClient(QTcpSocket* socket, QObject* parent = nullptr);
    qint64 bytesAvailable() override;
    QByteArray readAll() override;
    bool isOpen() override;
    bool isConnected() override;
    void close() override;
    qint64 write(const QByteArray& data) override;
    bool flush() override;
    bool waitForBytesWritten(int msecs) override;
    bool waitForReadyRead(int msecs) override;

private slots:
    void onDisconnected();
    void onConnected();
    void onReadyRead();

private:
    QTcpSocket* m_socket = nullptr;
};
