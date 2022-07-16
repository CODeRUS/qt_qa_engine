// Copyright (c) 2019-2020 Open Mobile Platform LLC.
#include <qt_qa_engine/IEnginePlatform.h>
#include <qt_qa_engine/QAEngine.h>
#include <qt_qa_engine/QAMouseEngine.h>
#include <qt_qa_engine/QAPendingEvent.h>

#include <QMouseEvent>
#include <QKeyEvent>
#include <QTimer>
#include <QtMath>
#include <private/qvariantanimation_p.h>
#include <qpa/qwindowsysteminterface_p.h>

#include <QElapsedTimer>

#include <QDebug>
#include <QThread>

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(categoryMouseEngine, "autoqa.qaengine.mouse", QtWarningMsg)

namespace
{

int seleniumKeyToQt(ushort key)
{
    if (key >= 'A' && key <= 'Z')
    {
        return key;
    }

    if (key < 0xe000 || key > 0xe03d)
    {
        return 0;
    }

    key -= 0xe000;
    static int seleniumKeys[]{
        0, // u'\ue000'
        Qt::Key_Cancel,
        Qt::Key_Help,
        Qt::Key_Backspace,
        Qt::Key_Tab,
        Qt::Key_Clear,
        Qt::Key_Return,
        Qt::Key_Enter,
        Qt::Key_Shift,
        Qt::Key_Control,
        Qt::Key_Alt,
        Qt::Key_Pause,
        Qt::Key_Escape,
        Qt::Key_Space,
        Qt::Key_PageUp,
        Qt::Key_PageDown, // u'\ue00f'
        Qt::Key_End,
        Qt::Key_Home,
        Qt::Key_Left,
        Qt::Key_Up,
        Qt::Key_Right,
        Qt::Key_Down,
        Qt::Key_Insert,
        Qt::Key_Delete,
        Qt::Key_Semicolon,
        Qt::Key_Equal,
        Qt::Key_0,
        Qt::Key_1,
        Qt::Key_2,
        Qt::Key_3,
        Qt::Key_4,
        Qt::Key_5, // u'\ue01f'
        Qt::Key_6,
        Qt::Key_7,
        Qt::Key_8,
        Qt::Key_9,
        Qt::Key_Asterisk,
        Qt::Key_Plus,
        Qt::Key_Colon,
        Qt::Key_Minus,
        Qt::Key_Period,
        Qt::Key_Slash, // u'\ue029'
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        Qt::Key_F1, // u'\ue031'
        Qt::Key_F2,
        Qt::Key_F3,
        Qt::Key_F4,
        Qt::Key_F5,
        Qt::Key_F6,
        Qt::Key_F7,
        Qt::Key_F8,
        Qt::Key_F9,
        Qt::Key_F10,
        Qt::Key_F11,
        Qt::Key_F12,
        Qt::Key_Meta,
    };
    return seleniumKeys[key];
}

} // namespace

QAMouseEngine::QAMouseEngine(QObject* parent)
    : QObject(parent)
    , m_eta(new QElapsedTimer())
    , m_touchDevice(new QTouchDevice())
{
    qRegisterMetaType<Qt::KeyboardModifiers>();

    m_touchDevice->setCapabilities(QTouchDevice::Position | QTouchDevice::Area);
    m_touchDevice->setMaximumTouchPoints(10);
    m_touchDevice->setType(QTouchDevice::TouchScreen);
    m_touchDevice->setName(QStringLiteral("qainput"));

    QWindowSystemInterface::registerTouchDevice(m_touchDevice);

    m_eta->start();
}

bool QAMouseEngine::isRunning() const
{
    return m_touchPoints.size() > 0;
}

QAMouseEngine::MouseMode QAMouseEngine::mode()
{
    return m_mode;
}

void QAMouseEngine::setMode(QAMouseEngine::MouseMode mode)
{
    m_mode = mode;
}

QAPendingEvent* QAMouseEngine::click(const QPointF& point)
{
    QVariantList action = {
        QVariantMap{
            {"action", "press"},
            {
                "options",
                QVariantMap{
                    {"x", point.x()},
                    {"y", point.y()},
                },
            },
        },
        QVariantMap{
            {"action", "wait"},
            {
                "options",
                QVariantMap{
                    {"ms", 200},
                },
            },
        },
        QVariantMap{
            {"action", "release"},
        },
    };

    return performTouchAction(action);
}

QAPendingEvent* QAMouseEngine::pressAndHold(const QPointF& point, int delay)
{
    QVariantList action = {
        QVariantMap{
            {"action", "longPress"},
            {
                "options",
                QVariantMap{
                    {"x", point.x()},
                    {"y", point.y()},
                },
            },
        },
        QVariantMap{
            {"action", "wait"},
            {
                "options",
                QVariantMap{
                    {"ms", delay},
                },
            },
        },
        QVariantMap{
            {"action", "release"},
        },
    };

    return performTouchAction(action);
}

QAPendingEvent* QAMouseEngine::drag(const QPointF& pointA,
                                    const QPointF& pointB,
                                    int delay,
                                    int duration,
                                    int moveSteps,
                                    int releaseDelay)
{
    QVariantList action = {
        QVariantMap{
            {"action", "longPress"},
            {
                "options",
                QVariantMap{
                    {"x", pointA.x()},
                    {"y", pointA.y()},
                },
            },
        },
        QVariantMap{
            {"action", "wait"},
            {
                "options",
                QVariantMap{
                    {"ms", delay},
                },
            },
        },
        QVariantMap{
            {"action", "moveTo"},
            {
                "options",
                QVariantMap{
                    {"x", pointB.x()},
                    {"y", pointB.y()},
                    {"duration", duration},
                    {"steps", moveSteps},
                },
            },
        },
        QVariantMap{
            {"action", "wait"},
            {
                "options",
                QVariantMap{
                    {"ms", releaseDelay},
                },
            },
        },
        QVariantMap{
            {"action", "release"},
        },
    };

    return performTouchAction(action);
}

QAPendingEvent* QAMouseEngine::move(
    const QPointF& pointA, const QPointF& pointB, int duration, int moveSteps, int releaseDelay)
{
    QVariantList action = {
        QVariantMap{
            {"action", "press"},
            {
                "options",
                QVariantMap{
                    {"x", pointA.x()},
                    {"y", pointA.y()},
                },
            },
        },
        QVariantMap{
            {"action", "moveTo"},
            {
                "options",
                QVariantMap{
                    {"x", pointB.x()},
                    {"y", pointB.y()},
                    {"duration", duration},
                    {"steps", moveSteps},
                },
            },
        },
        QVariantMap{
            {"action", "wait"},
            {
                "options",
                QVariantMap{
                    {"ms", releaseDelay},
                },
            },
        },
        QVariantMap{
            {"action", "release"},
        },
    };

    return performTouchAction(action);
}

void QAMouseEngine::pressEnter()
{
    sendKeyPress('\n', Qt::Key_Enter);
    sendKeyRelease('\n', Qt::Key_Enter);
}

QAPendingEvent* QAMouseEngine::performMultiAction(const QVariantList& multiActions)
{
    QAPendingEvent* event = new QAPendingEvent(this);
    event->setProperty("finishedCount", 0);
    QReadWriteLock* lock = new QReadWriteLock();

    const int actionsSize = multiActions.size();
    for (const QVariant& multiActionVar : multiActions)
    {
        const QVariantList actions = multiActionVar.toList();

        EventWorker* worker = EventWorker::PerformTouchAction(actions, Qt::NoModifier, this);
        connect(worker, &EventWorker::pressed, this, &QAMouseEngine::onPressed);
        connect(worker, &EventWorker::moved, this, &QAMouseEngine::onMoved);
        connect(worker, &EventWorker::released, this, &QAMouseEngine::onReleased);
        connect(worker,
                &EventWorker::finished,
                event,
                [lock, event, actionsSize]()
                {
                    lock->lockForWrite();
                    int finishedCount = event->property("finishedCount").toInt();
                    event->setProperty("finishedCount", ++finishedCount);
                    lock->unlock();
                    if (finishedCount == actionsSize)
                    {
                        QMetaObject::invokeMethod(event, "setCompleted", Qt::QueuedConnection);
                        event->deleteLater();
                        delete lock;
                    }
                });
    }

    if (m_mode == TouchEventMode)
    {
        m_touchPoints.clear();
    }

    return event;
}

QAPendingEvent* QAMouseEngine::performTouchAction(const QVariantList& actions, Qt::KeyboardModifiers mods)
{
    QAPendingEvent* event = new QAPendingEvent(this);
    EventWorker* worker = EventWorker::PerformTouchAction(actions, mods, this);
    connect(worker, &EventWorker::pressed, this, &QAMouseEngine::onPressed);
    connect(worker, &EventWorker::moved, this, &QAMouseEngine::onMoved);
    connect(worker, &EventWorker::released, this, &QAMouseEngine::onReleased);
    connect(worker,
            &EventWorker::finished,
            event,
            [event]()
            {
                QMetaObject::invokeMethod(event, "setCompleted", Qt::QueuedConnection);
                event->deleteLater();
            });

    if (m_mode == TouchEventMode)
    {
        m_touchPoints.clear();
    }

    return event;
}

void QAMouseEngine::performChainActions(const QVariantList& actions)
{
    qCDebug(categoryMouseEngine) << Q_FUNC_INFO << actions;

    QVariantList pointerArgs;
    QVariantList keyArgs;

    for (const QVariant& paramsVar : qAsConst(actions))
    {
        const QVariantMap param = paramsVar.toMap();
        if (param.value(QStringLiteral("type")).toString() == QLatin1String("pointer"))
        {
            pointerArgs.append(param);
        }
        else if (param.value(QStringLiteral("type")).toString() == QLatin1String("key"))
        {
            keyArgs.append(param);
        }
    }

    QVariantList keyActions = keyArgs.first().toMap().value(QStringLiteral("actions")).toList();
    QVariantList mouseActions = pointerArgs.first().toMap().value(QStringLiteral("actions")).toList();

    qCDebug(categoryMouseEngine) << Q_FUNC_INFO << keyActions;
    qCDebug(categoryMouseEngine) << Q_FUNC_INFO << mouseActions;

    if (keyActions.size() != mouseActions.size())
    {
        qCWarning(categoryMouseEngine)
                  << Q_FUNC_INFO << "Found two actions lists with mismatching size!";
        return;
    }

    QPointF previousPoint;
    Qt::KeyboardModifiers mods;

    for (int i = 0; i < keyActions.size(); i++)
    {
        // key action
        {
            const auto& actionVar = keyActions.at(i);
            const QVariantMap action = actionVar.toMap();
            qDebug() << action;
            QKeyEvent::Type eventType = QKeyEvent::KeyPress;
            const QString type = action.value(QStringLiteral("type")).toString();
            if (type == QLatin1String("pause"))
            {
                const int duration = action.value(QStringLiteral("duration"), "0").toInt();
                if (duration > 0)
                {
                    QEventLoop loop;
                    QTimer::singleShot(duration, &loop, &QEventLoop::quit);
                    loop.exec();
                }
            }
            else
            {
                QString value = action.value(QStringLiteral("value")).toString();
                const int key = seleniumKeyToQt(value.at(0).unicode());
                bool keyUp = type == QLatin1String("keyUp");
                if (key)
                {
                    value = QString();

                    if (key == Qt::Key_Control)
                    {
                        mods ^= Qt::ControlModifier;
                    }
                    else if (key == Qt::Key_Alt)
                    {
                        mods ^= Qt::AltModifier;
                    }
                    else if (key == Qt::Key_Shift)
                    {
                        mods ^= Qt::ShiftModifier;
                    }
                    else if (key == Qt::Key_Meta)
                    {
                        mods ^= Qt::MetaModifier;
                    }
                    else if (key >= Qt::Key_0 && key <= Qt::Key_9)
                    {
                        mods ^= Qt::KeypadModifier;
                    }
                }
                if (keyUp)
                {
                    eventType = QKeyEvent::KeyRelease;
                }

                QKeyEvent event(eventType, key, mods, value);

                emit keyEvent(event);
            }
        }

        //mouse action
        {
            const auto& actionVar = mouseActions.at(i);
            const QVariantMap action = actionVar.toMap();
            qDebug() << action;
            const QString type = action.value(QStringLiteral("type")).toString();
            if (type == QLatin1String("pause"))
            {
                const int duration = action.value(QStringLiteral("duration"), "0").toInt();
                if (duration > 0)
                {
                    QEventLoop loop;
                    QTimer::singleShot(duration, &loop, &QEventLoop::quit);
                    loop.exec();
                }
            }
            else if (type == QLatin1String("pointerMove"))
            {
                int posX = action.value(QStringLiteral("x")).toInt();
                int posY = action.value(QStringLiteral("y")).toInt();

                if (action.contains(QStringLiteral("origin")))
                {

                    auto platform = QAEngine::instance()->getPlatform();
                    auto& origin = action.value(QStringLiteral("origin"));
                    QString itemId;
                    QVariantMap originMap = origin.toMap();
                    if (originMap.isEmpty())
                    {
                        itemId = origin.toString();
                    }
                    else
                    {
                        itemId = originMap.value(originMap.firstKey()).toString();
                    }

                    if (auto item =
                            platform->getObject(itemId))
                    {
                        const auto point = platform->getAbsGeometry(item).center();
                        posX = point.x();
                        posY = point.y();
                    }
                }

                if (!previousPoint.isNull())
                {
                    QPointF point(posX, posY);

                    QVariantList action = {
                        QVariantMap{
                            {"action", "moveTo"},
                            {
                                "options",
                                QVariantMap{
                                    {"x", posX},
                                    {"y", posY},
                                    {"duration", 200},
                                    {"steps", 20},
                                },
                            },
                        },
                    };

                    performTouchAction(action, mods);
                }

                previousPoint = QPointF(posX, posY);
            }
            else if (type == QLatin1String("pointerDown"))
            {
                QVariantList action = {
                    QVariantMap{
                        {"action", "press"},
                        {
                            "options",
                            QVariantMap{
                                {"x", previousPoint.x()},
                                {"y", previousPoint.y()},
                            },
                        },
                    },
                    QVariantMap{
                        {"action", "wait"},
                        {
                            "options",
                            QVariantMap{
                                {"ms", 200},
                            },
                        },
                    },
                };

                performTouchAction(action, mods);
            }
            else if (type == QLatin1String("pointerUp"))
            {
                QVariantList action = {
                    QVariantMap{
                        {"action", "release"},
                        {
                            "options",
                            QVariantMap{
                                {"x", previousPoint.x()},
                                {"y", previousPoint.y()},
                            },
                        },
                    },
                };

                performTouchAction(action, mods);

                previousPoint = QPointF();
            }
        }
    }
}

int QAMouseEngine::getNextPointId()
{
    return ++m_tpId;
}

qint64 QAMouseEngine::getEta()
{
    return m_eta->elapsed();
}

QTouchDevice* QAMouseEngine::getTouchDevice()
{
    return m_touchDevice;
}

void QAMouseEngine::onPressed(const QPointF point, Qt::KeyboardModifiers mods)
{
    qCDebug(categoryMouseEngine) << Q_FUNC_INFO << point;
    if (m_mode == TouchEventMode)
    {
        int pointId = getNextPointId();

        QTouchEvent::TouchPoint tp(pointId);
        tp.setState(Qt::TouchPointPressed);

        QRectF rect(point.x() - 16, point.y() - 16, 32, 32);
        tp.setRect(rect);
        tp.setSceneRect(rect);
        tp.setScreenRect(rect);
        tp.setLastPos(point);
        tp.setStartPos(point);
        tp.setPressure(1);
        m_touchPoints.insert(sender(), tp);

        Qt::TouchPointStates states = tp.state();
        QEvent::Type type = QEvent::TouchBegin;
        if (m_touchPoints.size() > 1)
        {
            type = QEvent::TouchUpdate;
            states |= Qt::TouchPointStationary;
        }

        QTouchEvent te(type, m_touchDevice, mods, states, m_touchPoints.values());
        te.setTimestamp(m_eta->elapsed());

        tp.setState(Qt::TouchPointStationary);
        m_touchPoints.insert(sender(), tp);

        emit touchEvent(te);
    }
    else
    {
        QMouseEvent me(
            QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton, mods);
        me.setTimestamp(m_eta->elapsed());
        emit mouseEvent(me);
    }
}

void QAMouseEngine::onMoved(const QPointF point, Qt::KeyboardModifiers mods)
{
    qCDebug(categoryMouseEngine) << Q_FUNC_INFO << point;
    if (m_mode == TouchEventMode)
    {
        QTouchEvent::TouchPoint tp = m_touchPoints.value(sender());

        tp.setState(Qt::TouchPointMoved);

        QRectF rect(point.x() - 16, point.y() - 16, 32, 32);
        tp.setRect(rect);
        tp.setSceneRect(rect);
        tp.setScreenRect(rect);
        tp.setLastPos(tp.pos());
        tp.setStartPos(point);
        tp.setPressure(1);
        m_touchPoints.insert(sender(), tp);

        Qt::TouchPointStates states = tp.state();
        QEvent::Type type = QEvent::TouchUpdate;
        if (m_touchPoints.size() > 1)
        {
            states |= Qt::TouchPointStationary;
        }

        QTouchEvent te(type, m_touchDevice, mods, states, m_touchPoints.values());
        te.setTimestamp(m_eta->elapsed());

        tp.setState(Qt::TouchPointStationary);
        m_touchPoints.insert(sender(), tp);

        emit touchEvent(te);
    }
    else
    {
        QMouseEvent me(QEvent::MouseMove, point, Qt::NoButton, Qt::LeftButton, mods);
        me.setTimestamp(m_eta->elapsed());
        emit mouseEvent(me);
    }
}

void QAMouseEngine::onReleased(const QPointF point, Qt::KeyboardModifiers mods)
{
    qCDebug(categoryMouseEngine) << Q_FUNC_INFO << point;
    if (m_mode == TouchEventMode)
    {
        QTouchEvent::TouchPoint tp = m_touchPoints.value(sender());

        tp.setState(Qt::TouchPointReleased);

        QRectF rect(point.x() - 16, point.y() - 16, 32, 32);
        tp.setRect(rect);
        tp.setSceneRect(rect);
        tp.setScreenRect(rect);
        tp.setLastPos(tp.pos());
        tp.setStartPos(point);
        tp.setPressure(0);
        m_touchPoints.insert(sender(), tp);

        Qt::TouchPointStates states = tp.state();
        QEvent::Type type = QEvent::TouchEnd;
        if (m_touchPoints.size() > 1)
        {
            type = QEvent::TouchUpdate;
            states |= Qt::TouchPointStationary;
        }

        QTouchEvent te(type, m_touchDevice, mods, states, m_touchPoints.values());
        te.setTimestamp(m_eta->elapsed());

        m_touchPoints.remove(sender());

        emit touchEvent(te);
    }
    else
    {
        QMouseEvent me(
            QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton, mods);
        me.setTimestamp(m_eta->elapsed());
        emit mouseEvent(me);
    }
}

void QAMouseEngine::sendKeyPress(const QChar &text, int key)
{
    qCDebug(categoryMouseEngine) << Q_FUNC_INFO << text << key;
    QKeyEvent event(QKeyEvent::KeyPress, key, Qt::NoModifier, QString(text));

    emit keyEvent(event);
}

void QAMouseEngine::sendKeyRelease(const QChar &text, int key)
{
    qCDebug(categoryMouseEngine) << Q_FUNC_INFO << text << key;
    QKeyEvent event(QKeyEvent::KeyRelease, key, Qt::NoModifier, QString(text));

    emit keyEvent(event);
}

EventWorker::EventWorker(const QVariantList& actions, Qt::KeyboardModifiers mods, QAMouseEngine* engine)
    : QObject(nullptr)
    , m_actions(actions)
    , m_engine(engine)
    , m_mods(mods)
{
}

EventWorker::~EventWorker()
{
}

EventWorker* EventWorker::PerformTouchAction(const QVariantList& actions, Qt::KeyboardModifiers mods, QAMouseEngine* engine)
{
    EventWorker* worker = new EventWorker(actions, mods, engine);
    QThread* thread = new QThread;
    connect(thread, &QThread::started, worker, &EventWorker::start);
    connect(worker, &EventWorker::finished, thread, &QThread::quit);
    connect(thread, &QThread::finished, worker, &EventWorker::deleteLater);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    worker->moveToThread(thread);
    thread->start();
    return worker;
}

void EventWorker::start()
{
    QPointF previousPoint;

    for (const QVariant& actionVar : m_actions)
    {
        const QVariantMap actionMap = actionVar.toMap();
        const QString action = actionMap.value(QStringLiteral("action")).toString();
        const QVariantMap options = actionMap.value(QStringLiteral("options")).toMap();

        if (action == QLatin1String("wait"))
        {
            const int delay = options.value(QStringLiteral("ms")).toInt();
            QEventLoop loop;
            QTimer::singleShot(delay, &loop, &QEventLoop::quit);
            loop.exec();
        }
        else if (action == QLatin1String("longPress"))
        {
            const int posX = options.value(QStringLiteral("x")).toInt();
            const int posY = options.value(QStringLiteral("y")).toInt();
            QPoint point(posX, posY);

            if (options.contains(QStringLiteral("element")))
            {
                auto platform = QAEngine::instance()->getPlatform();
                if (auto item =
                        platform->getObject(options.value(QStringLiteral("element")).toString()))
                {
                    point = platform->getAbsGeometry(item).center();
                }
            }

            sendPress(point);
            previousPoint = point;

            const int delay = options.value(QStringLiteral("duration")).toInt();
            QEventLoop loop;
            QTimer::singleShot(delay, &loop, &QEventLoop::quit);
            loop.exec();
        }
        else if (action == QLatin1String("press"))
        {
            const int posX = options.value(QStringLiteral("x")).toInt();
            const int posY = options.value(QStringLiteral("y")).toInt();
            QPoint point(posX, posY);

            if (options.contains(QStringLiteral("element")))
            {
                auto platform = QAEngine::instance()->getPlatform();
                if (auto item =
                        platform->getObject(options.value(QStringLiteral("element")).toString()))
                {
                    point = platform->getAbsGeometry(item).center();
                }
            }

            sendPress(point);
            previousPoint = point;
        }
        else if (action == QLatin1String("moveTo"))
        {
            const int posX = options.value(QStringLiteral("x")).toInt();
            const int posY = options.value(QStringLiteral("y")).toInt();
            QPoint point(posX, posY);

            if (options.contains(QStringLiteral("element")))
            {
                auto platform = QAEngine::instance()->getPlatform();
                if (auto item =
                        platform->getObject(options.value(QStringLiteral("element")).toString()))
                {
                    point = platform->getAbsGeometry(item).center();
                }
            }

            const int duration = options.value(QStringLiteral("duration"), 500).toInt();
            const int steps = options.value(QStringLiteral("steps"), 20).toInt();

            sendMove(previousPoint, point, duration, steps);
            previousPoint = point;
        }
        else if (action == QLatin1String("release"))
        {
            QPointF point = previousPoint;
            if (options.contains(QStringLiteral("x")) && options.contains(QStringLiteral("y")))
            {
                const int posX = options.value(QStringLiteral("x")).toInt();
                const int posY = options.value(QStringLiteral("y")).toInt();
                point = QPointF(posX, posY);
            }
            sendRelease(point);
        }
        else if (action == QLatin1String("tap"))
        {
            const int posX = options.value(QStringLiteral("x")).toInt();
            const int posY = options.value(QStringLiteral("y")).toInt();
            QPoint point(posX, posY);

            if (options.contains(QStringLiteral("element")))
            {
                auto platform = QAEngine::instance()->getPlatform();
                if (auto item =
                        platform->getObject(options.value(QStringLiteral("element")).toString()))
                {
                    point = platform->getAbsGeometry(item).center();
                }
            }

            const int count = options.value(QStringLiteral("count")).toInt();
            for (int i = 0; i < count; i++)
            {
                sendPress(point);
                QEventLoop loop;
                QTimer::singleShot(200, &loop, &QEventLoop::quit);
                loop.exec();
                sendRelease(point);
            }
            previousPoint = point;
        }
        else
        {
            qCWarning(categoryMouseEngine) << Q_FUNC_INFO << "Unknown action:" << action;
        }
    }

    emit finished();
}

void EventWorker::sendPress(const QPointF& point)
{
    qCDebug(categoryMouseEngine) << Q_FUNC_INFO << point;

    emit pressed(point, m_mods);
}

void EventWorker::sendRelease(const QPointF& point)
{
    qCDebug(categoryMouseEngine) << Q_FUNC_INFO << point;

    emit released(point, m_mods);
}

void EventWorker::sendRelease(const QPointF& point, int delay)
{
    qCDebug(categoryMouseEngine) << Q_FUNC_INFO << point << delay;

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, [this, point]() { sendRelease(point); });
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(delay);
    loop.exec();
}

void EventWorker::sendMove(const QPointF& point)
{
    qCDebug(categoryMouseEngine) << Q_FUNC_INFO << point;

    emit moved(point, m_mods);
}

void EventWorker::sendMove(const QPointF& previousPoint,
                           const QPointF& point,
                           int duration,
                           int moveSteps)
{
    qCDebug(categoryMouseEngine) << Q_FUNC_INFO << previousPoint << point << duration << moveSteps;

    float stepSize = 5.0f;

    const float stepX = qAbs(point.x() - previousPoint.x()) / moveSteps;
    const float stepY = qAbs(point.y() - previousPoint.y()) / moveSteps;

    if (stepX > 0 && stepX < stepSize)
    {
        moveSteps = qAbs(qRound(point.x() - previousPoint.x())) / stepSize;
    }
    else if (stepY > 0 && stepY < stepSize)
    {
        moveSteps = qAbs(qRound(point.y() - previousPoint.y())) / stepSize;
    }

    auto* interpolator = QVariantAnimationPrivate::getInterpolator(QMetaType::QPointF);

    QPointF pointA = previousPoint;
    for (int currentMoveStep = 0; currentMoveStep < moveSteps; currentMoveStep++)
    {
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        connect(&timer,
                &QTimer::timeout,
                this,
                [this, interpolator, &pointA, currentMoveStep, moveSteps, point, previousPoint]()
                {
                    float progress = static_cast<float>(currentMoveStep) / moveSteps;

                    QPointF pointB = interpolator(&previousPoint, &point, progress).toPointF();
                    sendMove(pointB);
                    pointA = pointB;
                });
        connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        timer.start(duration / moveSteps);
        loop.exec();
    }
    sendMove(point);
}
