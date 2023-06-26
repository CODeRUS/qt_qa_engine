#include <qt_qa_engine/GenericEnginePlatform.h>
#include <qt_qa_engine/ITransportClient.h>
#include <qt_qa_engine/QAEngine.h>
#include <qt_qa_engine/QAKeyMouseEngine.h>
#include <qt_qa_engine/QAPendingEvent.h>

#include <QClipboard>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMetaMethod>
#include <QRegularExpression>
#include <QRegExp>
#include <QStandardPaths>
#include <QTimer>
#include <QXmlStreamWriter>
#include <QVariant>

#if defined(MO_USE_QXMLPATTERNS)
#include <QXmlQuery>
#endif

#include <qpa/qwindowsysteminterface_p.h>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <private/qeventpoint_p.h>
#endif

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(categoryGenericEnginePlatform, "autoqa.qaengine.platform.generic", QtWarningMsg)
Q_LOGGING_CATEGORY(categoryGenericEnginePlatformFind, "autoqa.qaengine.platform.generic.find", QtWarningMsg)
Q_LOGGING_CATEGORY(categoryGenericEnginePlatformRaw, "autoqa.qaengine.platform.generic.raw", QtWarningMsg)

bool variantCompare(const QVariant &lhs, const QVariant &rhs)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    return lhs < rhs;
#else
    return QVariant::compare(lhs, rhs) == QPartialOrdering::Less;
#endif
}

bool variantPairCompare(const std::pair<QString, QVariant> &lhs, const std::pair<QString, QVariant> &rhs)
{
    return variantCompare(lhs.second, rhs.second);
}

GenericEnginePlatform::GenericEnginePlatform(QWindow* window)
    : IEnginePlatform(window)
    , m_rootWindow(window)
    , m_keyMouseEngine(new QAKeyMouseEngine(this))
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO;

    connect(m_keyMouseEngine, &QAKeyMouseEngine::touchEvent, this, &GenericEnginePlatform::onTouchEvent);
    connect(m_keyMouseEngine, &QAKeyMouseEngine::mouseEvent, this, &GenericEnginePlatform::onMouseEvent);
    connect(m_keyMouseEngine, &QAKeyMouseEngine::keyEvent, this, &GenericEnginePlatform::onKeyEvent);
    connect(m_keyMouseEngine, &QAKeyMouseEngine::wheelEvent, this, &GenericEnginePlatform::onWheelEvent);
}

QWindow* GenericEnginePlatform::window()
{
    return m_rootWindow;
}

QObject* GenericEnginePlatform::rootObject()
{
    return m_rootObject;
}

void GenericEnginePlatform::socketReply(ITransportClient* socket, const QVariant& value, int status)
{
    QByteArray data;
    {
        QJsonObject reply;
        reply.insert(QStringLiteral("status"), status);
        reply.insert(QStringLiteral("value"), QJsonValue::fromVariant(value));

        data = QJsonDocument(reply).toJson(QJsonDocument::Compact);
    }
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket << data.size() << "Reply is:";
//    qCDebug(categoryGenericEnginePlatformRaw).noquote() << data;

    socket->write(data);
    socket->flush();
}

void GenericEnginePlatform::elementReply(ITransportClient* socket,
                                         QObjectList elements,
                                         bool multiple)
{
    QVariantList value;
    for (QObject* item : elements)
    {
        if (!item)
        {
            continue;
        }
        const QString uId = uniqueId(item);
        m_items.insert(uId, item);

        QVariantMap element;
        element.insert(QStringLiteral("ELEMENT"), uId);
        value.append(element);
    }

    if (value.isEmpty())
    {
        socketReply(socket, QString());
    }
    else if (!multiple)
    {
        socketReply(socket, value.first());
    }
    else
    {
        socketReply(socket, value);
    }
}

void GenericEnginePlatform::addItem(QObject* o)
{
    Q_UNUSED(o)
}

void GenericEnginePlatform::removeItem(QObject* o)
{
    QHash<QString, QObject*>::iterator i = m_items.begin();
    while (i != m_items.end())
    {
        if (i.value() == o)
        {
            i = m_items.erase(i);
            break;
        }
        else
        {
            ++i;
        }
    }
}

void GenericEnginePlatform::findElement(ITransportClient* socket,
                                        const QString& strategy,
                                        const QString& selector,
                                        bool multiple,
                                        QObject* item)
{
    qCDebug(categoryGenericEnginePlatform)
        << Q_FUNC_INFO << socket << strategy << selector << multiple << item;

    QString fixStrategy = strategy;
    fixStrategy = fixStrategy.remove(QChar(u' ')).toLower();
    const QString methodName = QStringLiteral("findStrategy_%1").arg(fixStrategy);
    if (!QAEngine::metaInvoke(
            socket, this, methodName, {selector, multiple, QVariant::fromValue(item)}))
    {
        findByProperty(socket, strategy, selector, multiple, item);
    }
}

void GenericEnginePlatform::findByProperty(ITransportClient* socket,
                                           const QString& propertyName,
                                           const QVariant& propertyValue,
                                           bool multiple,
                                           QObject* parentItem)
{
    qCDebug(categoryGenericEnginePlatform)
        << Q_FUNC_INFO << socket << propertyName << propertyValue << multiple << parentItem;

    QObjectList items = findItemsByProperty(propertyName, propertyValue, parentItem);
    elementReply(socket, items, multiple);
}

QObject* GenericEnginePlatform::findItemById(const QString& id, QObject* parentItem)
{
    qCDebug(categoryGenericEnginePlatformFind) << Q_FUNC_INFO << id << parentItem;

    if (!parentItem)
    {
        parentItem = rootObject();
    }

    if (checkMatch(id, uniqueId(parentItem)))
    {
        return parentItem;
    }

    for (QObject* child : childrenList(parentItem))
    {
        QObject* recursiveItem = findItemById(id, child);
        if (recursiveItem)
        {
            return recursiveItem;
        }
    }
    return nullptr;
}

QObjectList GenericEnginePlatform::findItemsByObjectName(const QString& objectName,
                                                         QObject* parentItem)
{
    qCDebug(categoryGenericEnginePlatformFind) << Q_FUNC_INFO << objectName << parentItem;
    QObjectList items;

    if (!parentItem)
    {
        parentItem = rootObject();
    }

    if (checkMatch(objectName, parentItem->objectName()))
    {
        items.append(parentItem);
    }

    for (QObject* child : childrenList(parentItem))
    {
        QObjectList recursiveItems = findItemsByObjectName(objectName, child);
        items.append(recursiveItems);
    }
    return items;
}

QObjectList GenericEnginePlatform::findItemsByClassName(const QString& className,
                                                        QObject* parentItem)
{
    qCDebug(categoryGenericEnginePlatformFind) << Q_FUNC_INFO << className << parentItem;

    QObjectList items;

    if (!parentItem)
    {
        parentItem = rootObject();
    }

    if (checkMatch(className, getClassName(parentItem)))
    {
        items.append(parentItem);
    }

    for (QObject* child : childrenList(parentItem))
    {
        QObjectList recursiveItems = findItemsByClassName(className, child);
        items.append(recursiveItems);
    }
    return items;
}

QObjectList GenericEnginePlatform::findItemsByProperty(const QString& propertyName,
                                                       const QVariant& propertyValue,
                                                       QObject* parentItem)
{
    qCDebug(categoryGenericEnginePlatformFind)
        << Q_FUNC_INFO << propertyName << propertyValue << parentItem;

    QObjectList items;

    if (!parentItem)
    {
        parentItem = rootObject();
    }

    if (parentItem->property(propertyName.toLatin1().constData()) == propertyValue)
    {
        items.append(parentItem);
    }

    for (QObject* child : childrenList(parentItem))
    {
        QObjectList recursiveItems = findItemsByProperty(propertyName, propertyValue, child);
        items.append(recursiveItems);
    }
    return items;
}

QObjectList GenericEnginePlatform::findItemsByText(const QString& text,
                                                   bool partial,
                                                   QObject* parentItem)
{
    qCDebug(categoryGenericEnginePlatformFind) << Q_FUNC_INFO << text << partial << parentItem;

    QObjectList items;

    if (!parentItem)
    {
        parentItem = rootObject();
    }

    const QString& itemText = getText(parentItem);
    if ((partial && itemText.contains(text)) || (!partial && itemText == text))
    {
        items.append(parentItem);
    }

    for (QObject* child : childrenList(parentItem))
    {
        QObjectList recursiveItems = findItemsByText(text, partial, child);
        items.append(recursiveItems);
    }
    return items;
}

QObjectList GenericEnginePlatform::findItemsByXpath(const QString& xpath, QObject* parentItem)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << xpath << parentItem;

    QObjectList items;
#if defined(MO_USE_QXMLPATTERNS)
    if (!parentItem)
    {
        parentItem = rootObject();
    }

    QString out;
    QXmlStreamWriter writer(&out);
    writer.setAutoFormatting(true);
    writer.writeStartDocument();
    recursiveDumpXml(&writer, parentItem, 0);
    writer.writeEndDocument();

    QXmlQuery query;
    query.setFocus(out);
    query.setQuery(xpath);

    if (!query.isValid())
    {
        qCWarning(categoryGenericEnginePlatform) << Q_FUNC_INFO << "Query not valid:" << xpath;
        return items;
    }
    QString tempString;
    query.evaluateTo(&tempString);

    if (tempString.trimmed().isEmpty())
    {
        return items;
    }

    const QString resultData =
        QLatin1String("<results>") + tempString + QLatin1String("</results>");
    QXmlStreamReader reader(resultData);
    reader.readNextStartElement();
    while (!reader.atEnd())
    {
        reader.readNext();
        if (reader.isStartElement())
        {
            const QString elementId = reader.attributes().value(QStringLiteral("id")).toString();
            const QString address = elementId.section(QChar(u'x'), -1);
            const qulonglong integer = address.toULongLong(NULL, 16);
            QObject* item = reinterpret_cast<QObject*>(integer);
            items.append(item);
            reader.skipCurrentElement();
        }
    }
#endif
    return items;
}

QObjectList GenericEnginePlatform::filterVisibleItems(QObjectList items)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << items;

    QObjectList result;
    for (QObject* item : items)
    {
        if (!isItemVisible(item))
        {
            continue;
        }
        result.append(item);
    }
    return result;
}

bool GenericEnginePlatform::containsObject(const QString& elementId)
{
    return m_items.contains(elementId);
}

QObject* GenericEnginePlatform::getObject(const QString& elementId)
{
    return m_items.value(elementId);
}

QString GenericEnginePlatform::getText(QObject* item)
{
    static const char* textProperties[] = {
        "label",
        "title",
        "description",
        "placeholderText",
        "text",
        "value",
        "name",
        "toolTip",
    };

    for (const char* textProperty : textProperties)
    {
        if (item->metaObject()->indexOfProperty(textProperty) > 0)
        {
            const QString text = item->property(textProperty).toString();
            if (!text.isEmpty())
            {
                return text;
            }
        }
    }

    return QString();
}

QRect GenericEnginePlatform::getGeometry(QObject* item)
{
    return QRect(getPosition(item), getSize(item));
}

QRect GenericEnginePlatform::getAbsGeometry(QObject* item)
{
    return QRect(getAbsPosition(item), getSize(item));
}

QPoint GenericEnginePlatform::getClickPosition(QObject *item)
{
    return getAbsGeometry(item).center();
}

void GenericEnginePlatform::activateWindow()
{
    m_rootWindow->raise();
    m_rootWindow->requestActivate();
    m_rootWindow->setWindowState(Qt::WindowState::WindowActive);
}

QJsonObject GenericEnginePlatform::dumpObject(QObject* item, int depth)
{
    if (!item)
    {
        qCritical() << Q_FUNC_INFO << "No object!";
        return QJsonObject();
    }

    QJsonObject object;

    const QString className = getClassName(item);
    object.insert(QStringLiteral("classname"), QJsonValue(className));

    const QString id = uniqueId(item);
    object.insert(QStringLiteral("id"), QJsonValue(id));

    auto mo = item->metaObject();
    do
    {
        const QString moClassName = QString::fromLatin1(mo->className());
        std::vector<std::pair<QString, QVariant>> v;
        v.reserve(mo->propertyCount() - mo->propertyOffset());
        for (int i = mo->propertyOffset(); i < mo->propertyCount(); ++i)
        {
            const QString propertyName = QString::fromLatin1(mo->property(i).name());
            if (m_blacklistedProperties.contains(moClassName) &&
                m_blacklistedProperties.value(moClassName).contains(propertyName))
            {
                qCDebug(categoryGenericEnginePlatform)
                    << "Found blacklisted:" << moClassName << propertyName;
                continue;
            }

            v.emplace_back(propertyName, mo->property(i).read(item));
        }
        std::sort(v.begin(), v.end(), variantPairCompare);
        for (auto& i : v)
        {
            if (!object.contains(i.first) && i.second.canConvert<QString>())
            {
                object.insert(i.first, QJsonValue::fromVariant(i.second));
            }
        }
    } while ((mo = mo->superClass()));

    const QRect rect = getGeometry(item);
    object.insert(QStringLiteral("width"), QJsonValue(rect.width()));
    object.insert(QStringLiteral("height"), QJsonValue(rect.height()));
    object.insert(QStringLiteral("x"), QJsonValue(rect.x()));
    object.insert(QStringLiteral("y"), QJsonValue(rect.y()));
    object.insert(QStringLiteral("zDepth"), QJsonValue(depth));

    const QPoint abs = getAbsPosition(item);
    object.insert(QStringLiteral("abs_x"), QJsonValue(abs.x()));
    object.insert(QStringLiteral("abs_y"), QJsonValue(abs.y()));

    object.insert(QStringLiteral("objectName"), QJsonValue(item->objectName()));
    if (!object.contains(QLatin1String("enabled")))
    {
        object.insert(QStringLiteral("enabled"), QJsonValue(isItemEnabled(item)));
    }
    if (!object.contains(QLatin1String("visible")))
    {
        object.insert(QStringLiteral("visible"), QJsonValue(isItemVisible(item)));
    }

    object.insert(QStringLiteral("mainTextProperty"), getText(item));

    return object;
}

QJsonObject GenericEnginePlatform::recursiveDumpTree(QObject* rootItem, int depth)
{
    QJsonObject object = dumpObject(rootItem, depth);
    QJsonArray childArray;

    int z = 0;
    for (QObject* child : childrenList(rootItem))
    {
        QJsonObject childObject = recursiveDumpTree(child, ++z);
        childArray.append(QJsonValue(childObject));
    }
    object.insert(QStringLiteral("children"), QJsonValue(childArray));

    return object;
}

bool GenericEnginePlatform::recursiveDumpXml(QXmlStreamWriter* writer, QObject* rootItem, int depth)
{
    const QString className = getClassName(rootItem);
    writer->writeStartElement(className);

    const QString id = uniqueId(rootItem);
    writer->writeAttribute(QStringLiteral("id"), id);

    const QRect abs = getAbsGeometry(rootItem);

    const QString bounds = QString("[%1,%2][%3,%4]")
                               .arg(abs.topLeft().x())
                               .arg(abs.topLeft().y())
                               .arg(abs.bottomRight().x())
                               .arg(abs.bottomRight().y());

    QStringList attributes;

    attributes.append(QStringLiteral("x"));
    writer->writeAttribute(QStringLiteral("x"), QString::number(abs.x()));

    attributes.append(QStringLiteral("y"));
    writer->writeAttribute(QStringLiteral("y"), QString::number(abs.y()));

    attributes.append(QStringLiteral("bounds"));
    writer->writeAttribute(QStringLiteral("bounds"), bounds);

    attributes.append(QStringLiteral("objectName"));
    writer->writeAttribute(QStringLiteral("objectName"), rootItem->objectName());

    attributes.append(QStringLiteral("className"));
    writer->writeAttribute(QStringLiteral("className"), className);

    attributes.append(QStringLiteral("index"));
    writer->writeAttribute(QStringLiteral("index"), QString::number(depth));

    auto mo = rootItem->metaObject();
    do
    {
        for (int i = mo->propertyOffset(); i < mo->propertyCount(); ++i)
        {
            const QString propertyName = QString::fromLatin1(mo->property(i).name());
            if (m_blacklistedProperties.contains(className) &&
                m_blacklistedProperties.value(className).contains(propertyName))
            {
                qCDebug(categoryGenericEnginePlatform)
                    << "Found blacklisted:" << className << propertyName;
                continue;
            }
            if (!attributes.contains(propertyName))
            {
                attributes.append(propertyName);
                QVariant value = mo->property(i).read(rootItem);
                if (value.canConvert<QString>())
                {
                    writer->writeAttribute(propertyName, value.toString());
                }
            }
        }
    } while ((mo = mo->superClass()));

    QString text = getText(rootItem);
    writer->writeAttribute(QStringLiteral("mainTextProperty"), text);

    if (!text.isEmpty())
    {
        writer->writeCharacters(text);
    }

    int z = 0;

    auto children = childrenList(rootItem);
    for (auto&& i : children)
    {
        if (recursiveDumpXml(writer, i, z))
        {
            z++;
        }
    }

    writer->writeEndElement();

    return true;
}

void GenericEnginePlatform::clickItem(QObject* item)
{
    const QPoint clickPos = getClickPosition(item);
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << item << clickPos;

    clickPoint(clickPos.x(), clickPos.y());
}

QString GenericEnginePlatform::getClassName(QObject* item)
{
    return QString::fromLatin1(item->metaObject()->className())
        .section(QChar(u'_'), 0, 0)
        .section(QChar(u':'), -1);
}

QString GenericEnginePlatform::uniqueId(QObject* item)
{
    return QStringLiteral("%1_0x%2")
        .arg(getClassName(item))
        .arg(reinterpret_cast<quintptr>(item), QT_POINTER_SIZE * 2, 16, QLatin1Char('0'));
}

void GenericEnginePlatform::setProperty(ITransportClient* socket,
                                        const QString& property,
                                        const QVariant& value,
                                        const QString& elementId)
{
    qCDebug(categoryGenericEnginePlatform)
        << Q_FUNC_INFO << socket << property << value << elementId;

    QObject* item = getObject(elementId);
    if (item)
    {
        item->setProperty(property.toLatin1().constData(), value);
        socketReply(socket, QString());
    }
    else
    {
        socketReply(socket, QString(), 1);
    }
}

void GenericEnginePlatform::clickPoint(float posx, float posy)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << posx << posy;

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(50);
    connect(m_keyMouseEngine->click(QPointF(posx, posy)),
            &QAPendingEvent::completed,
            &timer,
            static_cast<void (QTimer::*)()>(&QTimer::start));
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    loop.exec();
}

void GenericEnginePlatform::clickPoint(const QPoint& pos)
{
    clickPoint(pos.x(), pos.y());
}

void GenericEnginePlatform::pressAndHold(float posx, float posy, int delay)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << posx << posy << delay;

    QEventLoop loop;
    connect(m_keyMouseEngine->pressAndHold(QPointF(posx, posy), delay),
            &QAPendingEvent::completed,
            &loop,
            &QEventLoop::quit);
    loop.exec();
}

void GenericEnginePlatform::mouseMove(float startx, float starty, float stopx, float stopy)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << startx << starty << stopx << stopy;

    QEventLoop loop;
    QTimer timer;
    timer.setInterval(800);
    timer.setSingleShot(true);
    connect(m_keyMouseEngine->move(QPointF(startx, starty), QPointF(stopx, stopy)),
            &QAPendingEvent::completed,
            &timer,
            static_cast<void (QTimer::*)()>(&QTimer::start));
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    loop.exec();
}

void GenericEnginePlatform::mouseDrag(
    float startx, float starty, float stopx, float stopy, int delay)
{
    qCDebug(categoryGenericEnginePlatform)
        << Q_FUNC_INFO << startx << starty << stopx << stopy << delay;

    QEventLoop loop;
    QTimer timer;
    timer.setInterval(800);
    timer.setSingleShot(true);
    connect(m_keyMouseEngine->drag(QPointF(startx, starty), QPointF(stopx, stopy), delay),
            &QAPendingEvent::completed,
            &timer,
            static_cast<void (QTimer::*)()>(&QTimer::start));
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    loop.exec();
}

void GenericEnginePlatform::processTouchActionList(const QVariant& actionListArg)
{
    int startX = 0;
    int startY = 0;
    int endX = 0;
    int endY = 0;
    int delay = 800;

    const QVariantList actions = actionListArg.toList();
    for (const QVariant& actionArg : actions)
    {
        const QVariantMap action = actionArg.toMap();
        const QString actionName = action.value(QStringLiteral("action")).toString();
        const QVariantMap options = action.value(QStringLiteral("options")).toMap();

        if (actionName == QLatin1String("wait"))
        {
            delay = options.value(QStringLiteral("ms")).toInt();
        }
        else if (actionName == QLatin1String("tap"))
        {
            const int tapX = options.value(QStringLiteral("x")).toInt();
            const int tapY = options.value(QStringLiteral("y")).toInt();
            clickPoint(tapX, tapY);
        }
        else if (actionName == QLatin1String("press"))
        {
            startX = options.value(QStringLiteral("x")).toInt();
            startY = options.value(QStringLiteral("y")).toInt();
        }
        else if (actionName == QLatin1String("moveTo"))
        {
            endX = options.value(QStringLiteral("x")).toInt();
            endY = options.value(QStringLiteral("y")).toInt();
        }
        else if (actionName == QLatin1String("release"))
        {
            mouseDrag(startX, startY, endX, endY, delay);
        }
        else if (actionName == QLatin1String("longPress"))
        {
            const QString elementId = options.value(QStringLiteral("element")).toString();
            if (!containsObject(elementId))
            {
                continue;
            }
            pressAndHoldItem(getObject(elementId), delay);
        }
    }
}

bool GenericEnginePlatform::waitForPropertyChange(QObject* item,
                                                  const QString& propertyName,
                                                  const QVariant& value,
                                                  int timeout)
{
    qCDebug(categoryGenericEnginePlatform)
        << Q_FUNC_INFO << item << propertyName << value << timeout;

    if (!item)
    {
        qCWarning(categoryGenericEnginePlatform) << "item is null" << item;
        return false;
    }
    int propertyIndex = item->metaObject()->indexOfProperty(propertyName.toLatin1().constData());
    if (propertyIndex < 0)
    {
        qCWarning(categoryGenericEnginePlatform)
            << Q_FUNC_INFO << item << "property" << propertyName << "is not valid!";
        return false;
    }
    const QMetaProperty prop = item->metaObject()->property(propertyIndex);
    const auto propValue = prop.read(item);
    bool isNullptr = value.canConvert<std::nullptr_t>();
    if (value.isValid() && !isNullptr && propValue == value)
    {
        return false;
    }
    if (!prop.hasNotifySignal())
    {
        qCWarning(categoryGenericEnginePlatform)
            << Q_FUNC_INFO << item << "property" << propertyName << "have on notifySignal!";
        return false;
    }
    QEventLoop loop;
    QTimer timer;
    item->setProperty("WaitForPropertyChangeEventLoop", QVariant::fromValue(&loop));
    item->setProperty("WaitForPropertyChangePropertyName", propertyName);
    item->setProperty("WaitForPropertyChangePropertyValue", value);
    const QMetaMethod propertyChanged =
        metaObject()->method(metaObject()->indexOfSlot("onPropertyChanged()"));
    connect(item, prop.notifySignal(), this, propertyChanged);
    connect(&timer, &QTimer::timeout, this, [&loop]() { loop.exit(1); });
    timer.start(timeout);
    int result = loop.exec();
    disconnect(item, prop.notifySignal(), this, propertyChanged);

    return result == 0;
}

bool GenericEnginePlatform::registerSignal(QObject *item, const QString &signalName)
{
    qCDebug(categoryGenericEnginePlatform)
        << Q_FUNC_INFO << item << signalName;

    if (!item)
    {
        qCWarning(categoryGenericEnginePlatform) << "item is null" << item;
        return false;
    }
    int signalIndex = item->metaObject()->indexOfSignal(signalName.toLatin1().constData());
    if (signalIndex < 0)
    {
        qCWarning(categoryGenericEnginePlatform)
            << Q_FUNC_INFO << item << "signal" << signalName << "is not valid!";
        return false;
    }
    const QMetaMethod prop = item->metaObject()->method(signalIndex);
    const QMetaMethod signalReceived =
        metaObject()->method(metaObject()->indexOfSlot("onSignalReceived()"));

    qCDebug(categoryGenericEnginePlatform)
        << Q_FUNC_INFO << "connecting:" << prop.methodSignature() << signalReceived.methodSignature();

    connect(item, prop, this, signalReceived);

    return true;
}

bool GenericEnginePlatform::unregisterSignal(QObject *item, const QString &signalName)
{
    qCDebug(categoryGenericEnginePlatform)
        << Q_FUNC_INFO << item << signalName;

    if (!item)
    {
        qCWarning(categoryGenericEnginePlatform) << "item is null" << item;
        return false;
    }
    int signalIndex = item->metaObject()->indexOfSignal(signalName.toLatin1().constData());
    if (signalIndex < 0)
    {
        qCWarning(categoryGenericEnginePlatform)
            << Q_FUNC_INFO << item << "signal" << signalName << "is not valid!";
        return false;
    }
    const QMetaMethod prop = item->metaObject()->method(signalIndex);
    const QMetaMethod signalReceived =
        metaObject()->method(metaObject()->indexOfSlot("onSignalReceived()"));

    qCDebug(categoryGenericEnginePlatform)
        << Q_FUNC_INFO << "disconnecting:" << prop.methodSignature() << signalReceived.methodSignature();

    disconnect(item, prop, this, signalReceived);

    return true;
}

bool GenericEnginePlatform::checkMatch(const QString& pattern, const QString& value)
{
    if (value.isEmpty())
    {
        return false;
    }
    if (pattern.startsWith('/'))
    {
        QRegExp rx(pattern);
        if (rx.exactMatch(value))
        {
            return true;
        }
        return false;
    }
    else if (pattern.contains('*'))
    {
        QRegExp rx(pattern);
        rx.setPatternSyntax(QRegExp::Wildcard);
        if (rx.exactMatch(value))
        {
            return true;
        }
        return false;
    }
    else
    {
        return value == pattern;
    }
}

void GenericEnginePlatform::execute(ITransportClient* socket,
                                    const QString& methodName,
                                    const QVariantList& params)
{
    bool handled = false;
    bool success = QAEngine::metaInvoke(socket, this, methodName, params, &handled);

    if (!handled || !success)
    {
        qCWarning(categoryGenericEnginePlatform) << Q_FUNC_INFO << methodName << "not handled!";
        socketReply(socket, QString());
    }
}

void GenericEnginePlatform::onPropertyChanged()
{
    QObject* item = sender();
    QEventLoop* loop = item->property("WaitForPropertyChangeEventLoop").value<QEventLoop*>();
    if (!loop)
    {
        return;
    }
    const QString propertyName = item->property("WaitForPropertyChangePropertyName").toString();
    const QVariant propertyValue = item->property("WaitForPropertyChangePropertyValue");
    if (!propertyValue.isValid())
    {
        loop->quit();
        return;
    }
    if (propertyValue.canConvert<std::nullptr_t>())
    {
        loop->quit();
        return;
    }
    const QVariant property = item->property(propertyName.toLatin1().constData());
    if (property == propertyValue)
    {
        loop->quit();
    }
}

void GenericEnginePlatform::onSignalReceived()
{
    QObject *item = sender();
    int signalIndex = senderSignalIndex();
    const QMetaObject *mo = item->metaObject();
    const QMetaMethod signal = mo->method(signalIndex);
    QString signalSig = QString::fromLatin1(mo->normalizedSignature(signal.methodSignature()));

    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << item << signalSig;

    QString key = QStringLiteral("%1_%2").arg(uniqueId(item)).arg(signalSig);

    if (m_signalCounter.contains(key)) {
        m_signalCounter[key] += 1;
        qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << "signal counter:" << m_signalCounter[key];
    } else {
        qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << "signal not registered";
    }
}

void GenericEnginePlatform::onTouchEvent(const QTouchEvent& event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    auto e = new QTouchEvent(event.type(), static_cast<const QPointingDevice*>(event.device()), event.modifiers(), event.points());

    auto *mut = QMutableTouchEvent::from(e);
    mut->setTarget(m_rootWindow);

    QCoreApplication::sendEvent(m_rootWindow, e);
#else
    QWindowSystemInterface::handleTouchEvent(
        m_rootWindow,
        event.timestamp(),
        event.device(),
        QWindowSystemInterfacePrivate::toNativeTouchPoints(event.touchPoints(), m_rootWindow),
        event.modifiers());
#endif
}

void GenericEnginePlatform::onMouseEvent(const QMouseEvent& event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << event.type() << event.position() << event.scenePosition() << event.button() << event.buttons() << event.modifiers();
#else
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << event.type() << event.localPos() << event.screenPos() << event.button() << event.buttons() << event.modifiers();
#endif
    QWindowSystemInterface::handleMouseEvent(m_rootWindow,
                                             event.timestamp(),
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                                             event.position(),
                                             event.scenePosition(),
#else
                                             event.localPos(),
                                             event.screenPos(),
#endif
                                             event.buttons(),
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
                                             event.button(),
                                             event.type(),
#endif
                                             event.modifiers(),
                                             Qt::MouseEventNotSynthesized);
}

void GenericEnginePlatform::onKeyEvent(const QKeyEvent& event)
{
    qCDebug(categoryGenericEnginePlatform)
        << Q_FUNC_INFO << event.type() << event.key() << event.modifiers() << event.text();

   QWindowSystemInterface::handleKeyEvent(
               m_rootWindow, event.type(), event.key(), event.modifiers(), event.text());
}

void GenericEnginePlatform::onWheelEvent(const QWheelEvent &event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    qCDebug(categoryGenericEnginePlatform)
        << Q_FUNC_INFO << event.type() << event.position() << event.scenePosition() << event.pixelDelta() << event.angleDelta() << event.buttons() << event.modifiers();

    QWindowSystemInterface::handleWheelEvent(
                m_rootWindow, event.position(), event.scenePosition(), event.pixelDelta(), event.angleDelta(), event.modifiers(), event.phase());
#else
   qCDebug(categoryGenericEnginePlatform)
       << Q_FUNC_INFO << event.type() << event.pos() << event.globalPos() << event.pixelDelta() << event.angleDelta() << event.buttons() << event.modifiers();

   QWindowSystemInterface::handleWheelEvent(
       m_rootWindow, event.pos(), event.globalPos(), event.pixelDelta(), event.angleDelta(), event.modifiers(), event.phase());
#endif
}

void GenericEnginePlatform::appConnectCommand(ITransportClient* socket)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket;
    socketReply(socket, QString());
}

void GenericEnginePlatform::appDisconnectCommand(ITransportClient* socket)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket;
    socketReply(socket, QString());
}

void GenericEnginePlatform::initializeCommand(ITransportClient* socket)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket;
}

void GenericEnginePlatform::activateAppCommand(ITransportClient* socket, const QString& appName)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket << appName;

    if (appName != QAEngine::processName())
    {
        qCWarning(categoryGenericEnginePlatform)
            << Q_FUNC_INFO << appName << "is not" << QAEngine::processName();
        socketReply(socket, QString(), 1);
        return;
    }

    if (!m_rootWindow)
    {
        qCWarning(categoryGenericEnginePlatform) << Q_FUNC_INFO << "No window!";
        return;
    }

    QWindow* appWindow = qGuiApp->topLevelWindows().last();
    appWindow->raise();
    appWindow->requestActivate();

    socketReply(socket, QString());
}

void GenericEnginePlatform::closeAppCommand(ITransportClient* socket, const QString& appName)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket << appName;

    if (appName != QAEngine::processName())
    {
        qCWarning(categoryGenericEnginePlatform)
            << Q_FUNC_INFO << appName << "is not" << QAEngine::processName();
        socketReply(socket, QString(), 1);
        return;
    }

    socketReply(socket, QString());
    qApp->quit();
}

void GenericEnginePlatform::closeWindowCommand(ITransportClient *socket)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket;

    m_rootWindow->close();

    socketReply(socket, QString());
}

void GenericEnginePlatform::queryAppStateCommand(ITransportClient* socket, const QString& appName)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket << appName;

    if (appName != QAEngine::processName())
    {
        qCWarning(categoryGenericEnginePlatform)
            << Q_FUNC_INFO << appName << "is not" << QAEngine::processName();
        socketReply(socket, QString(), 1);
        return;
    }

    if (!m_rootWindow)
    {
        qCWarning(categoryGenericEnginePlatform) << Q_FUNC_INFO << "No window!";
        return;
    }

    const bool isAppActive = m_rootWindow->isActive();
    socketReply(socket,
                isAppActive ? QStringLiteral("RUNNING_IN_FOREGROUND")
                            : QStringLiteral("RUNNING_IN_BACKGROUND"));
}

void GenericEnginePlatform::backgroundCommand(ITransportClient* socket, qlonglong seconds)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket << seconds;

    if (!m_rootWindow)
    {
        qCWarning(categoryGenericEnginePlatform) << Q_FUNC_INFO << "No window!";
        return;
    }

    m_rootWindow->showMinimized();
    m_rootWindow->lower();
    if (seconds > 0)
    {
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        timer.start(seconds * 1000);
        loop.exec();
        activateWindow();
    }

    socketReply(socket, QString());
}

void GenericEnginePlatform::getClipboardCommand(ITransportClient* socket)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket;

    socketReply(socket, QString::fromUtf8(qGuiApp->clipboard()->text().toUtf8().toBase64()));
}

void GenericEnginePlatform::setClipboardCommand(ITransportClient* socket, const QString& content)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket << content;

    qGuiApp->clipboard()->setText(content);
    socketReply(socket, QString());
}

void GenericEnginePlatform::findElementCommand(ITransportClient* socket,
                                               const QString& strategy,
                                               const QString& selector)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket << strategy << selector;

    findElement(socket, strategy, selector);
}

void GenericEnginePlatform::findElementsCommand(ITransportClient* socket,
                                                const QString& strategy,
                                                const QString& selector)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket << strategy << selector;

    findElement(socket, strategy, selector, true);
}

void GenericEnginePlatform::findElementFromElementCommand(ITransportClient* socket,
                                                          const QString& strategy,
                                                          const QString& selector,
                                                          const QString& elementId)
{
    qCDebug(categoryGenericEnginePlatform)
        << Q_FUNC_INFO << socket << strategy << selector << elementId;

    findElement(socket, strategy, selector, false, getObject(elementId));
}

void GenericEnginePlatform::findElementsFromElementCommand(ITransportClient* socket,
                                                           const QString& strategy,
                                                           const QString& selector,
                                                           const QString& elementId)
{
    qCDebug(categoryGenericEnginePlatform)
        << Q_FUNC_INFO << socket << strategy << selector << elementId;

    findElement(socket, strategy, selector, true, getObject(elementId));
}

void GenericEnginePlatform::getLocationCommand(ITransportClient* socket, const QString& elementId)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket << elementId;

    QObject* item = getObject(elementId);
    if (item)
    {
        QJsonObject reply;
        const QRect geometry = getAbsGeometry(item);
        reply.insert(QStringLiteral("centerx"), geometry.center().x());
        reply.insert(QStringLiteral("centery"), geometry.center().y());
        reply.insert(QStringLiteral("x"), geometry.x());
        reply.insert(QStringLiteral("y"), geometry.y());
        reply.insert(QStringLiteral("width"), geometry.width());
        reply.insert(QStringLiteral("height"), geometry.height());
        socketReply(socket, reply);
    }
    else
    {
        socketReply(socket, QString());
    }
}

void GenericEnginePlatform::getLocationInViewCommand(ITransportClient* socket,
                                                     const QString& elementId)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket << elementId;

    QObject* item = getObject(elementId);
    if (item)
    {
        QJsonObject reply;
        const QRect geometry = getGeometry(item);
        reply.insert(QStringLiteral("centerx"), geometry.center().x());
        reply.insert(QStringLiteral("centery"), geometry.center().y());
        reply.insert(QStringLiteral("x"), geometry.x());
        reply.insert(QStringLiteral("y"), geometry.y());
        reply.insert(QStringLiteral("width"), geometry.width());
        reply.insert(QStringLiteral("height"), geometry.height());
        socketReply(socket, reply);
    }
    else
    {
        socketReply(socket, QString());
    }
}

void GenericEnginePlatform::getAttributeCommand(ITransportClient* socket,
                                                const QString& attribute,
                                                const QString& elementId)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket << attribute << elementId;

    QObject* item = getObject(elementId);
    if (item)
    {
        QVariant reply;
        if (attribute == "abs_x")
        {
            auto a = getAbsGeometry(item);
            reply = a.x();
        }
        else if (attribute == "abs_y")
        {
            auto a = getAbsGeometry(item);
            reply = a.y();
        }
        else if (attribute == "mainTextProperty")
        {
            reply = getText(item);
        }
        else if (attribute == "className")
        {
            reply = getClassName(item);
        }
        else if (attribute == "objectName")
        {
            reply = item->objectName();
        }
        else
        {
            reply = item->property(attribute.toLatin1().constData());
        }
        socketReply(socket, reply);
    }
    else
    {
        socketReply(socket, QString());
    }
}

void GenericEnginePlatform::getPropertyCommand(ITransportClient* socket,
                                               const QString& attribute,
                                               const QString& elementId)
{
    getAttributeCommand(socket, attribute, elementId);
}

void GenericEnginePlatform::getTextCommand(ITransportClient* socket, const QString& elementId)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket << elementId;

    QObject* item = getObject(elementId);
    if (item)
    {
        socketReply(socket, getText(item));
    }
    else
    {
        socketReply(socket, QString());
    }
}

void GenericEnginePlatform::getElementScreenshotCommand(ITransportClient* socket,
                                                        const QString& elementId)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket << elementId;

    QObject* item = getObject(elementId);
    if (item)
    {
        grabScreenshot(socket, item, true);
    }
    else
    {
        socketReply(socket, QString(), 1);
    }
}

void GenericEnginePlatform::getScreenshotCommand(ITransportClient* socket)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket;

    grabScreenshot(socket, m_rootObject, true);
}

void GenericEnginePlatform::getWindowRectCommand(ITransportClient* socket)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO;

    QJsonObject reply;
    const QRect geometry = m_rootWindow->geometry();
    reply.insert(QStringLiteral("centerx"), geometry.center().x());
    reply.insert(QStringLiteral("centery"), geometry.center().y());
    reply.insert(QStringLiteral("x"), geometry.x());
    reply.insert(QStringLiteral("y"), geometry.y());
    reply.insert(QStringLiteral("width"), geometry.width());
    reply.insert(QStringLiteral("height"), geometry.height());
    socketReply(socket, reply);
}

void GenericEnginePlatform::elementEnabledCommand(ITransportClient* socket,
                                                  const QString& elementId)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket << elementId;

    getAttributeCommand(socket, QStringLiteral("enabled"), elementId);
}

void GenericEnginePlatform::elementDisplayedCommand(ITransportClient* socket,
                                                    const QString& elementId)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket << elementId;

    getAttributeCommand(socket, QStringLiteral("visible"), elementId);
}

void GenericEnginePlatform::elementSelectedCommand(ITransportClient* socket,
                                                   const QString& elementId)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket << elementId;

    getAttributeCommand(socket, QStringLiteral("checked"), elementId);
}

void GenericEnginePlatform::getSizeCommand(ITransportClient* socket, const QString& elementId)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket << elementId;

    QObject* item = getObject(elementId);
    if (item)
    {
        QJsonObject reply;
        const QSize size = getSize(item);
        reply.insert(QStringLiteral("width"), size.width());
        reply.insert(QStringLiteral("height"), size.height());
        socketReply(socket, reply);
    }
    else
    {
        socketReply(socket, QString(), 1);
    }
}

void GenericEnginePlatform::getElementRectCommand(ITransportClient* socket,
                                                  const QString& elementId)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket << elementId;

    QObject* item = getObject(elementId);
    if (item)
    {
        QJsonObject reply;
        const QRect geometry = getAbsGeometry(item);
        reply.insert(QStringLiteral("width"), geometry.width());
        reply.insert(QStringLiteral("height"), geometry.height());
        reply.insert(QStringLiteral("x"), geometry.x());
        reply.insert(QStringLiteral("y"), geometry.y());
        socketReply(socket, reply);
    }
    else
    {
        socketReply(socket, QString(), 1);
    }
}

void GenericEnginePlatform::setValueImmediateCommand(ITransportClient* socket,
                                                     const QVariantList& value,
                                                     const QString& elementId)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket << value << elementId;

    QStringList text;
    for (const QVariant& val : value)
    {
        text.append(val.toString());
    }

    setProperty(socket, QStringLiteral("text"), text.join(QString()), elementId);
}

void GenericEnginePlatform::replaceValueCommand(ITransportClient* socket,
                                                const QVariantList& value,
                                                const QString& elementId)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket << value << elementId;

    setValueImmediateCommand(socket, value, elementId);
}

void GenericEnginePlatform::setValueCommand(ITransportClient* socket,
                                            const QVariantList& value,
                                            const QString& elementId)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket << value << elementId;

    setValueImmediateCommand(socket, value, elementId);
}

void GenericEnginePlatform::setValueCommand(ITransportClient* socket,
                                            const QString& value,
                                            const QString& elementId)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket << value << elementId;

    setProperty(socket, QStringLiteral("text"), value, elementId);
}

void GenericEnginePlatform::clickCommand(ITransportClient* socket, const QString& elementId)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket << elementId;

    QObject* item = getObject(elementId);
    if (item)
    {
        clickItem(item);
        socketReply(socket, QString());
    }
    else
    {
        socketReply(socket, QString(), 1);
    }
}

void GenericEnginePlatform::clearCommand(ITransportClient* socket, const QString& elementId)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket << elementId;

    setProperty(socket, QStringLiteral("text"), QString(), elementId);
}

void GenericEnginePlatform::submitCommand(ITransportClient* socket, const QString& elementId)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket << elementId;

    m_keyMouseEngine->pressEnter();
    socketReply(socket, QString());
}

void GenericEnginePlatform::getPageSourceCommand(ITransportClient* socket)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket;

    QString out;
    QXmlStreamWriter writer(&out);
    writer.setAutoFormatting(false);
    writer.writeStartDocument();
    recursiveDumpXml(&writer, rootObject(), 0);
    writer.writeEndDocument();

    socketReply(socket, out);
}

void GenericEnginePlatform::backCommand(ITransportClient* socket)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket;
    socketReply(socket, QStringLiteral("not_implemented"), 405);
}

void GenericEnginePlatform::forwardCommand(ITransportClient* socket)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket;
    socketReply(socket, QStringLiteral("not_implemented"), 405);
}

void GenericEnginePlatform::getOrientationCommand(ITransportClient* socket)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket;
    socketReply(socket, QStringLiteral("not_implemented"), 405);
}

void GenericEnginePlatform::setOrientationCommand(ITransportClient* socket,
                                                  const QString& orientation)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket << orientation;
    socketReply(socket, QStringLiteral("not_implemented"), 405);
}

void GenericEnginePlatform::hideKeyboardCommand(ITransportClient* socket,
                                                const QString& strategy,
                                                const QString& key,
                                                qlonglong keyCode,
                                                const QString& keyName)
{
    qCDebug(categoryGenericEnginePlatform)
        << Q_FUNC_INFO << socket << strategy << key << keyCode << keyName;
    socketReply(socket, QStringLiteral("not_implemented"), 405);
}

void GenericEnginePlatform::getCurrentActivityCommand(ITransportClient* socket)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket;
    socketReply(socket, QStringLiteral("not_implemented"), 405);
}

void GenericEnginePlatform::implicitWaitCommand(ITransportClient* socket, qlonglong msecs)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket << msecs;
    socketReply(socket, QStringLiteral("not_implemented"), 405);
}

void GenericEnginePlatform::activeCommand(ITransportClient* socket)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket;
    socketReply(socket, QStringLiteral("not_implemented"), 405);
}

void GenericEnginePlatform::getAlertTextCommand(ITransportClient* socket)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket;
    socketReply(socket, QStringLiteral("not_implemented"), 405);
}

void GenericEnginePlatform::isKeyboardShownCommand(ITransportClient* socket)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket;
    socketReply(socket, QStringLiteral("not_implemented"), 405);
}

void GenericEnginePlatform::activateIMEEngineCommand(ITransportClient* socket,
                                                     const QVariant& engine)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket << engine;
    socketReply(socket, QStringLiteral("not_implemented"), 405);
}

void GenericEnginePlatform::availableIMEEnginesCommand(ITransportClient* socket)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket;
    socketReply(socket, QStringLiteral("not_implemented"), 405);
}

void GenericEnginePlatform::getActiveIMEEngineCommand(ITransportClient* socket)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket;
    socketReply(socket, QStringLiteral("not_implemented"), 405);
}

void GenericEnginePlatform::deactivateIMEEngineCommand(ITransportClient* socket)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket;
    socketReply(socket, QStringLiteral("not_implemented"), 405);
}

void GenericEnginePlatform::isIMEActivatedCommand(ITransportClient* socket)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket;
    socketReply(socket, QStringLiteral("not_implemented"), 405);
}

void GenericEnginePlatform::keyeventCommand(ITransportClient* socket,
                                            const QVariant& keycodeArg,
                                            const QVariant& metaState,
                                            const QVariant& sessionIDArg,
                                            const QVariant& flagsArg)
{
    qCDebug(categoryGenericEnginePlatform)
        << Q_FUNC_INFO << socket << keycodeArg << metaState << sessionIDArg << flagsArg;
    socketReply(socket, QStringLiteral("not_implemented"), 405);
}

void GenericEnginePlatform::longPressKeyCodeCommand(ITransportClient* socket,
                                                    const QVariant& keycodeArg,
                                                    const QVariant& metaState,
                                                    const QVariant& flagsArg)
{
    qCDebug(categoryGenericEnginePlatform)
        << Q_FUNC_INFO << socket << keycodeArg << metaState << flagsArg;
    socketReply(socket, QStringLiteral("not_implemented"), 405);
}

void GenericEnginePlatform::pressKeyCodeCommand(ITransportClient* socket,
                                                const QVariant& keycodeArg,
                                                const QVariant& metaState,
                                                const QVariant& flagsArg)
{
    qCDebug(categoryGenericEnginePlatform)
        << Q_FUNC_INFO << socket << keycodeArg << metaState << flagsArg;
    socketReply(socket, QStringLiteral("not_implemented"), 405);
}

void GenericEnginePlatform::executeCommand(ITransportClient* socket,
                                           const QString& command,
                                           const QVariantList& params)
{
    qWarning() << Q_FUNC_INFO << socket << command << params;

    const QString fixCommand = QString(command).replace(QChar(':'), QChar('_'));
    const QString methodName = QStringLiteral("executeCommand_%1").arg(fixCommand);
    execute(socket, methodName, params);
}

void GenericEnginePlatform::executeAsyncCommand(ITransportClient* socket,
                                                const QString& command,
                                                const QVariantList& params)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket << command << params;

    const QString fixCommand = QString(command).replace(QChar(':'), QChar('_'));
    const QString methodName =
        QStringLiteral("executeCommand_%1").arg(fixCommand); // executeCommandAsync_ ?
    execute(socket, methodName, params);
}

void GenericEnginePlatform::performTouchCommand(ITransportClient* socket, const QVariant& paramsArg)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket << paramsArg;

    QEventLoop loop;
    connect(m_keyMouseEngine->performTouchAction(paramsArg.toList()),
            &QAPendingEvent::completed,
            &loop,
            &QEventLoop::quit);
    loop.exec();

    socketReply(socket, QString());
}

void GenericEnginePlatform::performMultiActionCommand(ITransportClient* socket,
                                                      const QVariant& paramsArg)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket << paramsArg;

    QEventLoop loop;
    connect(m_keyMouseEngine->performMultiAction(paramsArg.toList()),
            &QAPendingEvent::completed,
            &loop,
            &QEventLoop::quit);
    loop.exec();

    socketReply(socket, QString());
}

void GenericEnginePlatform::performActionsCommand(ITransportClient* socket,
                                                  const QVariant& paramsArg)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket;

    const QVariantList params = paramsArg.toList();
    if (params.size() == 0)
    {
        socketReply(socket, QString());
        return;
    }

    QEventLoop loop;
    connect(m_keyMouseEngine->performChainActions(params),
            &QAPendingEvent::completed,
            &loop,
            &QEventLoop::quit);
    loop.exec();

    socketReply(socket, QString());
}

void GenericEnginePlatform::getTimeoutsCommand(ITransportClient* socket)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket;

    QJsonObject reply{
        {"command", 3600000},
        {"implicit", 0},
    };
    socketReply(socket, reply);
}

void GenericEnginePlatform::findStrategy_id(ITransportClient* socket,
                                            const QString& selector,
                                            bool multiple,
                                            QObject* parentItem)
{
    QObject* item = findItemById(selector, parentItem);
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << selector << multiple << item;
    elementReply(socket, {item}, multiple);
}

void GenericEnginePlatform::findStrategy_objectName(ITransportClient* socket,
                                                    const QString& selector,
                                                    bool multiple,
                                                    QObject* parentItem)
{
    QObjectList items = findItemsByObjectName(selector, parentItem);
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << selector << multiple << items;
    elementReply(socket, items, multiple);
}

void GenericEnginePlatform::findStrategy_classname(ITransportClient* socket,
                                                   const QString& selector,
                                                   bool multiple,
                                                   QObject* parentItem)
{
    QObjectList items = findItemsByClassName(selector, parentItem);
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << selector << multiple << items;
    elementReply(socket, items, multiple);
}

void GenericEnginePlatform::findStrategy_name(ITransportClient* socket,
                                              const QString& selector,
                                              bool multiple,
                                              QObject* parentItem)
{
    QObjectList items = findItemsByText(selector, false, parentItem);
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << selector << multiple << items;
    elementReply(socket, items, multiple);
}

void GenericEnginePlatform::findStrategy_parent(ITransportClient* socket,
                                                const QString& selector,
                                                bool multiple,
                                                QObject* parentItem)
{
    const int depth = selector.toInt();
    QObject* pItem = getParent(parentItem);
    for (int i = 0; i < depth; i++)
    {
        if (!pItem)
        {
            break;
        }
        pItem = getParent(pItem);
    }
    const QObjectList items = {pItem};
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << selector << multiple << items;
    elementReply(socket, items, multiple);
}

void GenericEnginePlatform::findStrategy_xpath(ITransportClient* socket,
                                               const QString& selector,
                                               bool multiple,
                                               QObject* parentItem)
{
    QObjectList items = findItemsByXpath(selector, parentItem);
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << selector << multiple << items;
    elementReply(socket, items, multiple);
}

void GenericEnginePlatform::executeCommand_window_frameSize(ITransportClient *socket)
{
    const auto frameMargins = m_rootWindow->frameMargins();
    QJsonObject reply;
    reply.insert(QStringLiteral("left"), frameMargins.left());
    reply.insert(QStringLiteral("right"), frameMargins.right());
    reply.insert(QStringLiteral("top"), frameMargins.top());
    reply.insert(QStringLiteral("bottom"), frameMargins.bottom());
    socketReply(socket, reply);
}

void GenericEnginePlatform::executeCommand_app_method(ITransportClient* socket,
                                                      const QString& elementId,
                                                      const QString& method,
                                                      const QVariantList& params)
{
    qCDebug(categoryGenericEnginePlatform)
        << Q_FUNC_INFO << socket << elementId << method << params;

    QObject* item = getObject(elementId);
    if (!item)
    {
        socketReply(socket, QString("no item"), 1);
        return;
    }

    QVariant reply;
    bool implemented = false;
    bool result = QAEngine::metaInvokeRet(item, method, params, &implemented, &reply);

    qCDebug(categoryGenericEnginePlatform)
        << Q_FUNC_INFO << "app:method result:" << (result ? "true" : "false");
    socketReply(socket, result ? reply : false, result ? 0 : 1);
}

void GenericEnginePlatform::executeCommand_app_dumpTree(ITransportClient* socket)
{
    qCDebug(categoryGenericEnginePlatform) << Q_FUNC_INFO << socket;


    QJsonObject reply = recursiveDumpTree(rootObject());
    socketReply(socket,
                qCompress(QJsonDocument(reply).toJson(QJsonDocument::Compact), 9).toBase64());
}

void GenericEnginePlatform::executeCommand_app_setAttribute(ITransportClient* socket,
                                                            const QString& elementId,
                                                            const QString& attribute,
                                                            const QVariant& value)
{
    qCDebug(categoryGenericEnginePlatform)
        << Q_FUNC_INFO << socket << elementId << attribute << value;

    setProperty(socket, attribute, value, elementId);
}

void GenericEnginePlatform::executeCommand_app_waitForPropertyChange(ITransportClient* socket,
                                                                     const QString& elementId,
                                                                     const QString& propertyName,
                                                                     const QVariant& value,
                                                                     qlonglong timeout)
{
    qCDebug(categoryGenericEnginePlatform)
        << Q_FUNC_INFO << socket << elementId << propertyName << value << timeout;

    QObject* item = getObject(elementId);
    if (item)
    {
        bool result = waitForPropertyChange(item, propertyName, value, timeout);
        socketReply(socket, result);
    }
    else
    {
        socketReply(socket, QString(), 1);
    }
}

void GenericEnginePlatform::executeCommand_app_registerSignal(ITransportClient *socket, const QString &elementId, const QString &signalName)
{
    QObject* item = getObject(elementId);
    if (!item) {
        socketReply(socket, QString(), 1);
        return;
    }

    int ret = registerSignal(item, signalName) ? 0 : 1;
    if (ret == 0) {
        QString key = QStringLiteral("%1_%2").arg(uniqueId(item)).arg(signalName);
        m_signalCounter[key] = 0;
    }

    socketReply(socket, QString(), ret);
}

void GenericEnginePlatform::executeCommand_app_unregisterSignal(ITransportClient *socket, const QString &elementId, const QString &signalName)
{
    QObject* item = getObject(elementId);
    if (!item) {
        socketReply(socket, QString(), 1);
        return;
    }

    int ret = unregisterSignal(item, signalName) ? 0 : 1;
    if (ret == 0) {
        QString key = QStringLiteral("%1_%2").arg(uniqueId(item)).arg(signalName);
        m_signalCounter.remove(key);
    }

    socketReply(socket, QString(), ret);
}

void GenericEnginePlatform::executeCommand_app_countSignals(ITransportClient *socket, const QString &elementId, const QString &signalName)
{
    QObject* item = getObject(elementId);
    if (!item) {
        socketReply(socket, QString(), 1);
        return;
    }

    int count = -1;

    QString key = QStringLiteral("%1_%2").arg(uniqueId(item)).arg(signalName);
    if (m_signalCounter.contains(key)) {
        count = m_signalCounter[key];
    }

    socketReply(socket, count);
}

void GenericEnginePlatform::executeCommand_app_setLoggingFilter(ITransportClient* socket,
                                                                const QString& rules)
{
    QLoggingCategory::setFilterRules(rules);

    socketReply(socket, QString());
}

static QString fileLoggerPath;

void fileHandler(QtMsgType type, const QMessageLogContext&, const QString& msg)
{
    static QFile outFile;
    if (!outFile.isOpen())
    {
        if (fileLoggerPath.isEmpty())
        {
            QDir d(QStandardPaths::writableLocation(QStandardPaths::TempLocation));
            if (!d.exists())
            {
                d.mkpath(d.absolutePath());
            }
            auto appDir = QFileInfo(qApp->applicationFilePath()).fileName();
            d.mkdir(appDir);
            d.cd(appDir);
            fileLoggerPath = d.absoluteFilePath("app_log.txt");
        }
        outFile.setFileName(fileLoggerPath);
        outFile.open(QIODevice::WriteOnly | QIODevice::Append);
    }
    static QTextStream ts(&outFile);

    QString txt;
    switch (type)
    {
        case QtDebugMsg:
            txt = QString("Debug: %1").arg(msg);
            break;
        case QtWarningMsg:
            txt = QString("Warning: %1").arg(msg);
            break;
        case QtCriticalMsg:
            txt = QString("Critical: %1").arg(msg);
            break;
        case QtFatalMsg:
            txt = QString("Fatal: %1").arg(msg);
            break;
        case QtInfoMsg:
            txt = QString("Info: %1").arg(msg);
            break;
    }
    ts << txt << Qt::endl;
    outFile.flush();
}

void GenericEnginePlatform::executeCommand_app_installFileLogger(ITransportClient* socket,
                                                                 const QString& filePath)
{
    static bool fileLoggerInstalled = false;
    if (!fileLoggerInstalled)
    {
        fileLoggerPath = filePath;
        qInstallMessageHandler(fileHandler);
        fileLoggerInstalled = true;
    }
    socketReply(socket, QString());
}

void GenericEnginePlatform::executeCommand_app_click(ITransportClient *socket, double mousex, double mousey)
{
    clickPoint(mousex, mousey);
    socketReply(socket, QString());
}

void GenericEnginePlatform::executeCommand_app_exit(ITransportClient* socket, double code)
{
    socketReply(socket, QString());
    qApp->exit(code);
}

void GenericEnginePlatform::executeCommand_app_crash(ITransportClient *socket)
{
    socketReply(socket, QString());
    QTimer *p = reinterpret_cast<QTimer*>(qApp + 2000);
    p->start(9999);
    p->setInterval(2222);
}

void GenericEnginePlatform::executeCommand_app_pressAndHold(ITransportClient *socket, double mousex, double mousey)
{
    pressAndHold(mousex, mousey, 1500);
    socketReply(socket, QString());
}

void GenericEnginePlatform::executeCommand_app_move(ITransportClient *socket, double fromx, double fromy, double tox, double toy)
{
    mouseMove(fromx, fromy, tox, toy);
    socketReply(socket, QString());
}

void GenericEnginePlatform::executeCommand_app_listSignals(ITransportClient *socket, const QString &elementId)
{
    qCDebug(categoryGenericEnginePlatform)
        << Q_FUNC_INFO << socket << elementId;

    QObject* object = getObject(elementId);
    if (!object)
    {
        socketReply(socket, QString("no item"), 1);
        return;
    }

    QStringList result;

    auto mo = object->metaObject();
    do
    {
        for (int i = mo->methodOffset(); i < mo->methodOffset() + mo->methodCount(); i++)
        {
            const QMetaMethod method = mo->method(i);
            const QByteArray sig = mo->normalizedSignature(method.methodSignature());
            if (mo->indexOfSignal(sig) >= 0) {
                result.append(QString::fromLatin1(sig));
            }
        }
    } while ((mo = mo->superClass()));

    socketReply(socket, result);
}
