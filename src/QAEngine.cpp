// Copyright (c) 2019-2020 Open Mobile Platform LLC.
#include <qt_qa_engine/IEnginePlatform.h>
#include <qt_qa_engine/ITransportClient.h>
#include <qt_qa_engine/QAEngine.h>
#include <qt_qa_engine/QAEngineSocketClient.h>
#include <qt_qa_engine/TCPSocketServer.h>

#if defined(MO_USE_QUICK)
#include <QQuickWindow>
#include <qt_qa_engine/QuickEnginePlatform.h>
#endif

#if defined(MO_USE_QWIDGETS)
#include <QApplication>
#include <qt_qa_engine/WidgetsEnginePlatform.h>
#endif

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaMethod>
#include <QTcpSocket>
#include <QTimer>
#include <QWindow>

#include <QGuiApplication>

#include <QLoggingCategory>

#include <private/qhooks_p.h>
#include <private/qmetaobject_p.h>
#include <private/qmetatype_p.h>

#include <array>

Q_LOGGING_CATEGORY(categoryEngine, "autoqa.qaengine.engine", QtWarningMsg)

namespace
{

bool s_engineLoaded = false;
QAEngine* s_instance = nullptr;

QString s_processName = "qaengine";
bool s_exiting = false;

QHash<QWindow*, IEnginePlatform*> s_windows;
QWindow* s_lastFocusWindow = nullptr;

inline QGenericArgument qVariantToArgument(const QVariant& variant)
{
    if (variant.isValid() && !variant.isNull())
    {
        return QGenericArgument(variant.typeName(), variant.constData());
    }
    return QGenericArgument();
}

} // namespace

bool QAEngine::isLoaded()
{
    return s_instance;
}

void QAEngine::objectCreated(QObject* o)
{
    if (!s_instance)
    {
        return;
    }

    s_instance->addItem(o);
}

void QAEngine::objectRemoved(QObject* o)
{
    if (!s_instance)
    {
        return;
    }

    s_instance->removeItem(o);
}

IEnginePlatform* QAEngine::getPlatform(bool silent)
{
    if (s_windows.contains(s_lastFocusWindow))
    {
        return s_windows.value(s_lastFocusWindow);
    }
    else
    {
        if (!silent)
        {
            qCWarning(categoryEngine)
                << Q_FUNC_INFO << "No platform for window:" << s_lastFocusWindow;
        }
    }
    return nullptr;
}

void QAEngine::initializeSocket()
{
    qCDebug(categoryEngine) << Q_FUNC_INFO << Qt::endl
                            << "name:" << qApp->arguments().first() << Qt::endl
                            << "pid:" << qApp->applicationPid();

    qCDebug(categoryEngine) << "QAPreload Version:"
#ifdef QAPRELOAD_VERSION
                            << QStringLiteral(QAPRELOAD_VERSION);
#else
                            << QStringLiteral("2.0.0-dev");
#endif

    m_socketServer->start();
}

void QAEngine::initializeEngine()
{
    disconnect(
        m_socketServer, &ITransportServer::commandReceived, this, &QAEngine::initializeEngine);

    if (s_engineLoaded)
    {
        return;
    }
    s_engineLoaded = true;

    qCDebug(categoryEngine) << Q_FUNC_INFO << Qt::endl;

    qtHookData[QHooks::RemoveQObject] = reinterpret_cast<quintptr>(&QAEngine::objectRemoved);
    qtHookData[QHooks::AddQObject] = reinterpret_cast<quintptr>(&QAEngine::objectCreated);

    connect(qApp,
            &QCoreApplication::aboutToQuit,
            []()
            {
                qCDebug(categoryEngine) << Q_FUNC_INFO << "about to quit!";
                s_exiting = true;
            });

    connect(qGuiApp, &QGuiApplication::focusWindowChanged, this, &QAEngine::onFocusWindowChanged);
    if (!qGuiApp->topLevelWindows().isEmpty())
    {
        onFocusWindowChanged(qGuiApp->topLevelWindows().first());
    }
    if (!qGuiApp->focusWindow())
    {
        QTimer* t = new QTimer();
        connect(t,
                &QTimer::timeout,
                [this, t]()
                {
                    if (!qGuiApp->focusWindow())
                    {
                        return;
                    }
                    onFocusWindowChanged(qGuiApp->focusWindow());
                    t->deleteLater();
                });
        t->start(1000);
    }
}

void QAEngine::onFocusWindowChanged(QWindow* window)
{
    qCDebug(categoryEngine) << "focusWindowChanged" << window << "appName"
                            << qApp->arguments().first();

    if (!window)
    {
        return;
    }

    if (!s_windows.contains(window))
    {
        IEnginePlatform* platform = nullptr;
#if defined(MO_USE_QUICK)
        if (qobject_cast<QQuickWindow*>(window))
        {
            platform = new QuickEnginePlatform(window);
        }
#endif

#if defined(MO_USE_QWIDGETS)
        if (!platform)
        {
            platform = new WidgetsEnginePlatform(window);
        }
#endif
        if (!platform)
        {
            qWarning() << Q_FUNC_INFO << "Can't determine platform";
            return;
        }
        connect(platform, &IEnginePlatform::ready, this, &QAEngine::onPlatformReady);
        connect(window,
                &QWindow::destroyed,
                [window]()
                {
                    if (!s_exiting)
                    {
                        s_windows.remove(window);
                    }
                });
        QTimer::singleShot(0, platform, &IEnginePlatform::initialize);

        s_windows.insert(window, platform);
    }

    s_lastFocusWindow = window;
}

void QAEngine::onPlatformReady()
{

    IEnginePlatform* platform = qobject_cast<IEnginePlatform*>(sender());
    if (!platform)
    {
        return;
    }

    s_processName = QFileInfo(qApp->arguments().first()).baseName();

    qCDebug(categoryEngine) << Q_FUNC_INFO << "Process name:" << s_processName
                            << "platform:" << platform << platform->window()
                            << platform->rootObject();
}

void QAEngine::clientLost(ITransportClient* client)
{
    qCDebug(categoryEngine) << Q_FUNC_INFO << client;
}

QAEngine* QAEngine::instance()
{
    if (!s_instance)
    {
        s_instance = new QAEngine(qApp);
    }
    return s_instance;
}

QAEngine::QAEngine(QObject* parent)
    : QObject(parent)
    , m_socketServer(new TCPSocketServer(8888, this))
{
    qRegisterMetaType<QTcpSocket*>();
    qRegisterMetaType<ITransportClient*>();
    qRegisterMetaType<ITransportServer*>();
    QLoggingCategory::setFilterRules("autoqa.qaengine.*.debug=false\n"
                                     "autoqa.qaengine.transport.server.debug=true");

    connect(m_socketServer, &ITransportServer::commandReceived, this, &QAEngine::initializeEngine);
    connect(m_socketServer, &ITransportServer::commandReceived, this, &QAEngine::processCommand);
    connect(m_socketServer, &ITransportServer::clientLost, this, &QAEngine::clientLost);
}

QAEngine::~QAEngine()
{
}

QString QAEngine::processName()
{
    return s_processName;
}

bool QAEngine::metaInvoke(ITransportClient* socket,
                          QObject* object,
                          const QString& methodName,
                          const QVariantList& params,
                          bool* implemented)
{
    auto mo = object->metaObject();
    do
    {
        for (int i = mo->methodOffset(); i < mo->methodOffset() + mo->methodCount(); i++)
        {
            if (mo->method(i).name() == methodName.toLatin1())
            {
                if (implemented)
                {
                    *implemented = true;
                }

                const QMetaMethod method = mo->method(i);

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                QVariantList args = params;

                std::vector<const void*> data;
                std::vector<const char*> names;
                std::vector<const QtPrivate::QMetaTypeInterface*> types;

                QMetaMethodReturnArgument r = {};
                data.push_back(r.data);
                names.push_back(r.name);
                types.push_back(r.metaType);

                auto socketArg = Q_ARG(ITransportClient*, socket);
                data.push_back(socketArg.data);
                names.push_back(socketArg.name);
                types.push_back(socketArg.metaType);

                for (int i = 0; i < (method.parameterCount() - 1) && args.count() > i; i++) {
                    QMetaType paramType = method.parameterMetaType(i + 1);
                    if (args[i].metaType() != paramType) {
                        if (args[i].canConvert(paramType)) {
                            args[i].convert(paramType);
                        } else if (paramType == QMetaType(QMetaType::Type::QVariant)) {
                            args[i] = QVariant::fromValue(args[i]);
                        } else {
                            qWarning() << Q_FUNC_INFO << "Can't convert" << args[i].metaType() << args[i] << "to" << paramType;
                            return false;
                        }
                    }
                    data.push_back(args[i].data());
                    names.push_back(paramType.name());
                    types.push_back(paramType.iface());
                }

                QMetaMethodInvoker::InvokeFailReason reason =
                    QMetaMethodInvoker::invokeImpl(method, object, Qt::DirectConnection,
                                                   data.size(), &data[0], &names[0], &types[0]);

                if (int(reason) <= 0) {
                    return reason == QMetaMethodInvoker::InvokeFailReason::None;
                } else {
                    qWarning() << Q_FUNC_INFO << "method not found!";
                    return false;
                }
#else
                QGenericArgument arguments[9] = {QGenericArgument()};
                for (int i = 0; i < (method.parameterCount() - 1) && params.count() > i; i++)
                {
                    if (method.parameterType(i + 1) == QMetaType::QVariant)
                    {
                        arguments[i] = Q_ARG(QVariant, params[i]);
                    }
                    else
                    {
                        arguments[i] = qVariantToArgument(params[i]);
                    }
                }

                return QMetaObject::invokeMethod(object,
                                                 methodName.toLatin1().constData(),
                                                 Qt::DirectConnection,
                                                 Q_ARG(ITransportClient*, socket),
                                                 arguments[0],
                                                 arguments[1],
                                                 arguments[2],
                                                 arguments[3],
                                                 arguments[4],
                                                 arguments[5],
                                                 arguments[6],
                                                 arguments[7],
                                                 arguments[8]);
#endif
            }
        }
    } while ((mo = mo->superClass()));

    if (implemented)
    {
        *implemented = false;
    }
    return false;
}


bool QAEngine::metaInvokeRet(QObject* object,
                             const QString& methodName,
                             const QVariantList& params,
                             bool* implemented,
                             QVariant *ret)
{
    auto mo = object->metaObject();
    do
    {
        for (int i = mo->methodOffset(); i < mo->methodOffset() + mo->methodCount(); i++)
        {
            if (mo->method(i).name() == methodName.toLatin1())
            {
                if (implemented)
                {
                    *implemented = true;
                }

                const QMetaMethod method = mo->method(i);
                QVariantList args = params;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)

                std::vector<const void*> data;
                std::vector<const char*> names;
                std::vector<const QtPrivate::QMetaTypeInterface*> types;

                QMetaMethodReturnArgument r = {};
                const QMetaType returnType = method.returnMetaType();
                if (!ret || returnType == QMetaType(QMetaType::Type::Void)) {
                    data.push_back(r.data);
                    names.push_back(r.name);
                    types.push_back(r.metaType);
                } else if (ret) {
                    ret->setValue(QVariant(returnType, ret->data()));

                    data.push_back(ret->data());
                    names.push_back(returnType.name());
                    types.push_back(returnType.iface());
                }

                for (int i = 0; i < method.parameterCount() && args.count() > i; i++) {
                    QMetaType paramType = method.parameterMetaType(i);
                    if (args[i].metaType() != paramType) {
                        if (args[i].canConvert(paramType)) {
                            args[i].convert(paramType);
                        } else if (paramType == QMetaType(QMetaType::Type::QVariant)) {
                            args[i] = QVariant::fromValue(args[i]);
                        } else {
                            qWarning() << Q_FUNC_INFO << "Can't convert" << args[i].metaType() << args[i] << "to" << paramType;
                            return false;
                        }
                    }
                    data.push_back(args[i].data());
                    names.push_back(paramType.name());
                    types.push_back(paramType.iface());
                }

                QMetaMethodInvoker::InvokeFailReason reason =
                    QMetaMethodInvoker::invokeImpl(method, object, Qt::DirectConnection,
                                                   data.size(), &data[0], &names[0], &types[0]);

                if (int(reason) <= 0) {
                    return reason == QMetaMethodInvoker::InvokeFailReason::None;
                } else {
                    qWarning() << Q_FUNC_INFO << "method not found!";
                    return false;
                }
#else
                QGenericArgument arguments[10] = {QGenericArgument()};
                for (int i = 0; i < method.parameterCount() && args.count() > i; i++)
                {
                    int paramType = method.parameterType(i);
                    if (paramType == QMetaType::QVariant)
                    {
                        arguments[i] = Q_ARG(QVariant, args[i]);
                    }
                    else
                    {
                        if (paramType != args[i].type() && args[i].canConvert(paramType)) {
                            args[i].convert(paramType);
                        }
                        arguments[i] = qVariantToArgument(args[i]);
                    }
                }

                int returnType =  method.returnType();
                if (returnType == QMetaType::Void) {
                    return QMetaObject::invokeMethod(object,
                                                     methodName.toLatin1().constData(),
                                                     Qt::DirectConnection,
                                                     arguments[0],
                                                     arguments[1],
                                                     arguments[2],
                                                     arguments[3],
                                                     arguments[4],
                                                     arguments[5],
                                                     arguments[6],
                                                     arguments[7],
                                                     arguments[8],
                                                     arguments[9]);
                } else {
                    ret->setValue(QVariant(QVariant::nameToType(QMetaType::typeName(returnType))));
                    return QMetaObject::invokeMethod(object,
                                                     methodName.toLatin1().constData(),
                                                     Qt::DirectConnection,
                                                     QGenericReturnArgument(ret->typeName(), ret->data()),
                                                     arguments[0],
                                                     arguments[1],
                                                     arguments[2],
                                                     arguments[3],
                                                     arguments[4],
                                                     arguments[5],
                                                     arguments[6],
                                                     arguments[7],
                                                     arguments[8],
                                                     arguments[9]);
                }
#endif
            }
        }
    } while ((mo = mo->superClass()));

    if (implemented)
    {
        *implemented = false;
    }
    return false;
}

void QAEngine::addItem(QObject* o)
{
    if (auto platform = getPlatform(true))
    {
        platform->addItem(o);
    }
}

void QAEngine::removeItem(QObject* o)
{
    if (s_exiting)
    {
        return;
    }
    if (auto platform = getPlatform(true))
    {
        platform->removeItem(o);
    }
    if (s_lastFocusWindow == o)
    {
        s_lastFocusWindow = qGuiApp->topLevelWindows().first();
    }
}

void QAEngine::processCommand(ITransportClient* socket, const QByteArray& cmd)
{
    qCDebug(categoryEngine) << Q_FUNC_INFO << socket;
    qCDebug(categoryEngine).noquote() << cmd;

    QJsonParseError error;
    QJsonDocument json = QJsonDocument::fromJson(cmd, &error);
    if (error.error != QJsonParseError::NoError)
    {
        return;
    }

    auto&& object = json.object();

    if (!object.contains(QStringLiteral("cmd")))
    {
        return;
    }

    if (object.value(QStringLiteral("cmd")).toVariant().toString() != QStringLiteral("action"))
    {
        return;
    }

    if (!object.contains(QStringLiteral("action")))
    {
        return;
    }

    processAppiumCommand(socket,
                         object.value(QStringLiteral("action")).toVariant().toString(),
                         object.value(QStringLiteral("params")).toVariant().toList());
}

bool QAEngine::processAppiumCommand(ITransportClient* socket,
                                    const QString& action,
                                    const QVariantList& params)
{
    const QString methodName = QStringLiteral("%1Command").arg(action);
    qCDebug(categoryEngine) << Q_FUNC_INFO << socket << methodName << params;

    bool result = false;
    if (auto platform = getPlatform())
    {
        qCDebug(categoryEngine) << Q_FUNC_INFO << platform << platform->window()
                                << platform->rootObject();
        bool implemented = true;
        result = metaInvoke(socket, platform, methodName, params, &implemented);

        if (!implemented)
        {
            platform->socketReply(socket, QStringLiteral("not_implemented"), 405);
        }
    }
    else
    {
        QJsonObject reply;
        reply.insert(QStringLiteral("status"), 1);
        reply.insert(QStringLiteral("value"), QStringLiteral("no platform!"));

        const QByteArray data = QJsonDocument(reply).toJson(QJsonDocument::Compact);

        socket->write(data);
        socket->flush();
    }

    return result;
}
