// Copyright (c) 2019-2020 Open Mobile Platform LLC.
#include <qt_qa_engine/ITransportClient.h>
#include <qt_qa_engine/QAEngine.h>
#include <qt_qa_engine/QAKeyMouseEngine.h>
#include <qt_qa_engine/QuickEnginePlatform.h>

#include <QBuffer>
#include <QDebug>
#include <QGuiApplication>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QQmlExpression>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QQuickWindow>
#include <QScreen>
#include <QTimer>

#include <private/qquickitem_p.h>
#include <private/qquickwindow_p.h>

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(categoryQuickEnginePlatform, "autoqa.qaengine.platform.quick", QtWarningMsg)

QList<QObject*> QuickEnginePlatform::childrenList(QObject* parentItem)
{
    QList<QObject*> result;

    QQuickItem* quick = qobject_cast<QQuickItem*>(parentItem);
    if (!quick)
    {
        return result;
    }

    QQuickItemPrivate* p = QQuickItemPrivate::get(quick);
    if (!p)
    {
        return result;
    }

    for (QQuickItem* w : p->paintOrderChildItems())
    {
        result.append(w);
    }

    return result;
}

QQuickItem* QuickEnginePlatform::getItem(const QString& elementId)
{
    return qobject_cast<QQuickItem*>(getObject(elementId));
}

QuickEnginePlatform::QuickEnginePlatform(QWindow* window)
    : GenericEnginePlatform(window)
{
    Q_INIT_RESOURCE(qtqaengine);

//#if defined(Q_OS_ANDROID) || defined(MO_OS_IOS)
    m_keyMouseEngine->setMode(QAKeyMouseEngine::TouchEventMode);
//#endif
}

QQuickItem* QuickEnginePlatform::findParentFlickable(QQuickItem* rootItem)
{
    qCDebug(categoryQuickEnginePlatform) << Q_FUNC_INFO << rootItem;

    if (!rootItem)
    {
        return nullptr;
    }
    while (rootItem->parentItem())
    {
        if (rootItem->parentItem()->metaObject()->indexOfProperty("flickableDirection") >= 0)
        {
            return rootItem->parentItem();
        }
        rootItem = rootItem->parentItem();
    }

    return nullptr;
}

QVariantList QuickEnginePlatform::findNestedFlickable(QQuickItem* parentItem)
{
    qCDebug(categoryQuickEnginePlatform) << Q_FUNC_INFO << parentItem;

    QVariantList items;

    if (!parentItem)
    {
        parentItem = m_rootQuickItem;
    }

    QList<QQuickItem*> childItems = parentItem->childItems();
    for (QQuickItem* child : childItems)
    {
        if (child->metaObject()->indexOfProperty("flickableDirection") >= 0)
        {
            items.append(QVariant::fromValue(child));
        }
        QVariantList recursiveItems = findNestedFlickable(child);
        items.append(recursiveItems);
    }
    return items;
}

QQuickItem* QuickEnginePlatform::getApplicationWindow()
{
    qCDebug(categoryQuickEnginePlatform) << Q_FUNC_INFO;

    QQuickItem* applicationWindow = m_rootQuickItem;
    if (!qmlEngine(applicationWindow))
    {
        applicationWindow = applicationWindow->childItems().first();
    }
    return applicationWindow;
}

QPoint QuickEnginePlatform::getAbsPosition(QObject* item)
{
    qCDebug(categoryQuickEnginePlatform) << Q_FUNC_INFO << item;

    QQuickItem* q = qobject_cast<QQuickItem*>(item);
    if (!q)
    {
        return QPoint();
    }
    QPoint position = QPointF(q->x(), q->y()).toPoint();
    QPoint abs;
    if (q->parentItem())
    {
        abs = m_rootQuickItem->mapFromItem(q->parentItem(), position).toPoint();
    }
    else
    {
        abs = position;
    }
    return abs;
}

QPoint QuickEnginePlatform::getPosition(QObject* item)
{
    qCDebug(categoryQuickEnginePlatform) << Q_FUNC_INFO << item;

    QQuickItem* q = qobject_cast<QQuickItem*>(item);
    if (!q)
    {
        return QPoint();
    }
    return q->position().toPoint();
}

QSize QuickEnginePlatform::getSize(QObject* item)
{
    qCDebug(categoryQuickEnginePlatform) << Q_FUNC_INFO << item;

    QQuickItem* q = qobject_cast<QQuickItem*>(item);
    if (!q)
    {
        return QSize();
    }
    return QSize(q->width(), q->height());
}

bool QuickEnginePlatform::isItemEnabled(QObject* item)
{
    qCDebug(categoryQuickEnginePlatform) << Q_FUNC_INFO << item;

    QQuickItem* q = qobject_cast<QQuickItem*>(item);
    if (!q)
    {
        return false;
    }
    return q->isEnabled();
}

bool QuickEnginePlatform::isItemVisible(QObject* item)
{
    qCDebug(categoryQuickEnginePlatform) << Q_FUNC_INFO << item;

    QQuickItem* q = qobject_cast<QQuickItem*>(item);
    if (!q)
    {
        return false;
    }
    return q->isVisible();
}

QPointF QuickEnginePlatform::mapToGlobal(const QPointF &point)
{
    return m_rootQuickItem->mapToGlobal(point);
}

QVariant QuickEnginePlatform::executeJS(const QString& jsCode, QQuickItem* item)
{
    qCDebug(categoryQuickEnginePlatform) << Q_FUNC_INFO << jsCode << item;

    QQmlExpression expr(qmlEngine(item)->rootContext(), item, jsCode);
    bool isUndefined = false;
    const QVariant reply = expr.evaluate(&isUndefined);
    if (expr.hasError())
    {
        qCWarning(categoryQuickEnginePlatform) << Q_FUNC_INFO << expr.error().toString();
    }
    return isUndefined ? QVariant(QStringLiteral("undefined")) : reply;
}

QObject* QuickEnginePlatform::getParent(QObject* item)
{
    if (!item)
    {
        return nullptr;
    }
    QQuickItem* q = qobject_cast<QQuickItem*>(item);
    if (!q)
    {
        return item->parent();
    }
    return q->parentItem();
}

void QuickEnginePlatform::initialize()
{
    qCDebug(categoryQuickEnginePlatform) << Q_FUNC_INFO;

    if (!m_rootWindow)
    {
        qCDebug(categoryQuickEnginePlatform) << Q_FUNC_INFO << "No windows!";
        return;
    }

    QQuickWindow* qWindow = qobject_cast<QQuickWindow*>(m_rootWindow);

    if (!qWindow)
    {
        qCWarning(categoryQuickEnginePlatform)
            << Q_FUNC_INFO << m_rootWindow << "is not QQuickWindow!";
        return;
    }
    qCDebug(categoryQuickEnginePlatform) << Q_FUNC_INFO << qWindow;

    m_rootQuickWindow = qWindow;
    m_rootQuickItem = qWindow->contentItem();
    m_rootObject = m_rootQuickItem;

    // initialize touch indicator
    QQmlEngine* engine = getEngine();
    QQmlComponent component(engine, QUrl(QStringLiteral("qrc:/QAEngine/TouchIndicator.qml")));
    if (!component.isReady())
    {
        qWarning() << Q_FUNC_INFO << component.errorString();
        return;
    }
    QObject* object = component.create(engine->rootContext());
    if (!object)
    {
        qWarning() << Q_FUNC_INFO << component.errorString();
        return;
    }
    QQuickItem* item = qobject_cast<QQuickItem*>(object);
    if (!item)
    {
        qWarning() << Q_FUNC_INFO << object << "object is not QQuickitem!";
        return;
    }
    item->setParent(m_rootQuickItem);
    item->setParentItem(m_rootQuickItem);

    const auto childrens = childrenList(m_rootQuickItem);
    const auto lastChildren = qobject_cast<QQuickItem*>(childrens.last());

    item->setZ(lastChildren->z() + 1);

    m_touchIndicator = item;
    qGuiApp->installEventFilter(this);

    emit ready();
}

bool QuickEnginePlatform::eventFilter(QObject* watched, QEvent* event)
{
    switch (event->type())
    {
        case QEvent::TouchBegin:
        case QEvent::TouchUpdate:
        {
            QTouchEvent* te = static_cast<QTouchEvent*>(event);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            if (te->points().isEmpty())
#else
            if (te->touchPoints().isEmpty())
#endif
            {
                break;
            }
            QMetaObject::invokeMethod(
                m_touchIndicator,
                "show",
                Qt::QueuedConnection,
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                Q_ARG(QVariant, te->points().first().scenePosition().toPoint()),
                Q_ARG(QVariant, te->points().first().ellipseDiameters().toSize()));
#else
                Q_ARG(QVariant, te->touchPoints().first().screenPos().toPoint()),
                Q_ARG(QVariant, te->touchPoints().first().rect().size().toSize()));
#endif
            break;
        }
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonDblClick:
        case QEvent::MouseMove:
        {
            QMouseEvent* me = static_cast<QMouseEvent*>(event);
            if (me->buttons() == Qt::NoButton) {
                return QObject::eventFilter(watched, event);
            }
            QMetaObject::invokeMethod(m_touchIndicator,
                                      "show",
                                      Qt::QueuedConnection,
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                                      Q_ARG(QVariant, me->scenePosition()),
#else
                                      Q_ARG(QVariant, me->windowPos()),
#endif
                                      Q_ARG(QVariant, QSize(32, 32)));
            break;
        }
        case QEvent::TouchEnd:
        case QEvent::TouchCancel:
        case QEvent::MouseButtonRelease:
        {
            QMetaObject::invokeMethod(m_touchIndicator, "hide", Qt::QueuedConnection);
            break;
        }
        default:
            break;
    }
    return QObject::eventFilter(watched, event);
}

void QuickEnginePlatform::grabScreenshot(ITransportClient* socket,
                                         QObject* item,
                                         bool fillBackground)
{
    qCDebug(categoryQuickEnginePlatform) << Q_FUNC_INFO << socket << item << fillBackground;

    QQuickItem* q = qobject_cast<QQuickItem*>(item);
    if (!q)
    {
        return;
    }

    if (!q->window()->isVisible())
    {
        socketReply(socket, QString());
        return;
    }

    if (q == m_rootQuickItem)
    {
        QByteArray arr;
        QBuffer buffer(&arr);
        QPixmap pix;

        pix = QPixmap::fromImage(q->window()->grabWindow());

        if (fillBackground)
        {
            QPixmap pixmap(pix.width(), pix.height());
            QPainter painter(&pixmap);
            painter.fillRect(0, 0, pixmap.width(), pixmap.height(), Qt::black);
            painter.drawPixmap(0, 0, pix);
            pixmap.save(&buffer, "PNG");
        }
        else
        {
            pix.save(&buffer, "PNG");
        }

        socketReply(socket, arr.toBase64());
    }
    else
    {
        QSharedPointer<QQuickItemGrabResult> grabber = q->grabToImage();

        connect(grabber.data(),
                &QQuickItemGrabResult::ready,
                [this, grabber, socket, fillBackground]()
                {
                    QByteArray arr;
                    QBuffer buffer(&arr);
                    buffer.open(QIODevice::WriteOnly);
                    if (fillBackground)
                    {
                        QPixmap pixmap(grabber->image().width(), grabber->image().height());
                        QPainter painter(&pixmap);
                        painter.fillRect(0, 0, pixmap.width(), pixmap.height(), Qt::white);
                        painter.drawImage(0, 0, grabber->image());
                        pixmap.save(&buffer, "PNG");
                    }
                    else
                    {
                        grabber->image().save(&buffer, "PNG");
                    }
                    socketReply(socket, arr.toBase64());
                });
    }
}

void QuickEnginePlatform::pressAndHoldItem(QObject* qitem, int delay)
{
    qCDebug(categoryQuickEnginePlatform) << Q_FUNC_INFO << qitem << delay;

    if (!qitem)
    {
        return;
    }

    QQuickItem* item = qobject_cast<QQuickItem*>(qitem);
    if (!item)
    {
        return;
    }

    const QPointF itemAbs = getAbsPosition(item);
    pressAndHold(itemAbs.x() + item->width() / 2, itemAbs.y() + item->height() / 2, delay);
}

void QuickEnginePlatform::clearFocus()
{
    qCDebug(categoryQuickEnginePlatform) << Q_FUNC_INFO;

    QQuickWindowPrivate* wp = QQuickWindowPrivate::get(m_rootQuickWindow);
    wp->clearFocusObject();
}

void QuickEnginePlatform::clearComponentCache()
{
    qCDebug(categoryQuickEnginePlatform) << Q_FUNC_INFO;

    QQmlEngine* engine = getEngine();
    if (!engine)
    {
        return;
    }
    engine->clearComponentCache();
}

QQmlEngine* QuickEnginePlatform::getEngine(QQuickItem* item)
{
    if (!item)
    {
        item = m_rootQuickItem;
    }
    QQmlEngine* engine = qmlEngine(item);
    if (!engine)
    {
        engine = qmlEngine(item->childItems().first());
    }
    return engine;
}

void QuickEnginePlatform::executeCommand_touch_pressAndHold(ITransportClient* socket,
                                                            qlonglong posx,
                                                            qlonglong posy)
{
    qCDebug(categoryQuickEnginePlatform) << Q_FUNC_INFO << socket << posx << posy;

    pressAndHold(posx, posy);
    socketReply(socket, QString());
}

void QuickEnginePlatform::executeCommand_touch_mouseSwipe(
    ITransportClient* socket, qlonglong posx, qlonglong posy, qlonglong stopx, qlonglong stopy)
{
    qCDebug(categoryQuickEnginePlatform) << Q_FUNC_INFO << socket << posx << posy << stopx << stopy;

    mouseMove(posx, posy, stopx, stopy);
    socketReply(socket, QString());
}

void QuickEnginePlatform::executeCommand_touch_mouseDrag(
    ITransportClient* socket, qlonglong posx, qlonglong posy, qlonglong stopx, qlonglong stopy)
{
    qCDebug(categoryQuickEnginePlatform) << Q_FUNC_INFO << socket << posx << posy << stopx << stopy;

    mouseDrag(posx, posy, stopx, stopy);
    socketReply(socket, QString());
}

void QuickEnginePlatform::executeCommand_app_js(ITransportClient* socket,
                                                const QString& elementId,
                                                const QString& jsCode)
{
    qCDebug(categoryQuickEnginePlatform) << Q_FUNC_INFO << socket << elementId << jsCode;

    QQuickItem* item = getItem(elementId);
    if (!item)
    {
        socketReply(socket, QString());
        return;
    }

    QVariant result = executeJS(jsCode, item);
    qCDebug(categoryQuickEnginePlatform) << Q_FUNC_INFO << result;
    socketReply(socket, result);
}
