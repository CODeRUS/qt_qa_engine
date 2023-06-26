#pragma once

#include <QObject>
#include <QVariant>

class QAEngineSocketClient;
class ITransportClient;
class ITransportServer;
class IEnginePlatform;
class QWindow;
class QAEngine : public QObject
{
    Q_OBJECT
public:
    static QAEngine* instance();
    static bool isLoaded();
    static void objectCreated(QObject* o);
    static void objectRemoved(QObject* o);
    IEnginePlatform* getPlatform(bool silent = false);

    virtual ~QAEngine();

    static QString processName();
    static bool metaInvoke(ITransportClient* socket,
                           QObject* object,
                           const QString& methodName,
                           const QVariantList& params,
                           bool* implemented = nullptr);

    static bool metaInvokeRet(QObject* object,
                              const QString& methodName,
                              const QVariantList& params,
                              bool* implemented = nullptr,
                              QVariant *ret = nullptr);

    void addItem(QObject* o);
    void removeItem(QObject* o);

public slots:
    void initializeSocket();
    void initializeEngine();

private slots:
    void onFocusWindowChanged(QWindow* window);
    void processCommand(ITransportClient* socket, const QByteArray& cmd);
    bool processAppiumCommand(ITransportClient* socket,
                              const QString& action,
                              const QVariantList& params);
    void onPlatformReady();
    void clientLost(ITransportClient* client);

private:
    explicit QAEngine(QObject* parent = nullptr);
    ITransportServer* m_socketServer = nullptr;
};

