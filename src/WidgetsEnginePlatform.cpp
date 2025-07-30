#include <qt_qa_engine/ITransportClient.h>
#include <qt_qa_engine/QAEngine.h>
#include <qt_qa_engine/QAKeyMouseEngine.h>
#include <qt_qa_engine/WidgetsEnginePlatform.h>

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QBuffer>
#include <QComboBox>
#include <QDebug>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMetaMethod>
#include <QMetaObject>
#include <QTimer>
#include <QWidget>
#include <QWindow>
#include <QXmlStreamWriter>

#include <private/qapplication_p.h>
#include <private/qwidget_p.h>
#include <private/qwindow_p.h>

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(categoryWidgetsEnginePlatform, "autoqa.qaengine.platform.widgets", QtWarningMsg)
Q_LOGGING_CATEGORY(categoryWidgetsEnginePlatformFind, "autoqa.qaengine.platform.widgets.find", QtWarningMsg)

namespace
{

QList<QAction*> s_actions;
QHash<QAction*, QSet<QWidget*> > s_actionHash;
QHash<QMenu*, QWidget*> s_menuHash;
EventHandler* s_eventHandler = nullptr;

static bool s_registerPlatform = []()
{
    if (!s_eventHandler)
    {
        QTimer::singleShot(0,
                           qApp,
                           []()
                           {
                               s_eventHandler = new EventHandler(qApp);
                               qApp->installEventFilter(s_eventHandler);
                           });
    }
    return true;
}();

} // namespace

QList<QObject*> WidgetsEnginePlatform::childrenList(QObject* parentItem)
{
    if (parentItem == rootObject())
    {
        s_actionHash.clear();
        s_menuHash.clear();
    }

    QList<QObject*> result;

    // if (parentItem == m_rootWindow)
    // {
    //     result.append(m_rootWidget);
    // }

    QWidget* widget = qobject_cast<QWidget*>(parentItem);
    if (widget)
    {
        for (QAction* action : widget->actions())
        {
            s_actionHash[action].insert(widget);
            if (action->menu() && action->menu() != widget)
            {
                if (s_menuHash.contains(action->menu()))
                {
                    if (s_menuHash.value(action->menu()) != widget)
                    {
                        continue;
                    }
                }
                else
                {
                    s_menuHash.insert(action->menu(), widget);
                }
                if (action->menu()->isActiveWindow()) {
                    result.append(action->menu());
                }
            }
            result.append(action);
        }
    }
    QGraphicsScene *gs = qobject_cast<QGraphicsScene*>(parentItem);
    if (gs)
    {
        for (QGraphicsItem *gi : gs->items())
        {
            QGraphicsObject *gw = gi->toGraphicsObject();
            if (gw)
            {
                result.append(gw);
            }
        }
    }
    QGraphicsWidget *gw = qobject_cast<QGraphicsWidget*>(parentItem);
    if (gw)
    {
        for (QGraphicsItem *gi : gw->childItems())
        {
            QGraphicsObject *gw = gi->toGraphicsObject();
            if (gw)
            {
                result.append(gw);
            }
        }
    }
    for (QObject *o : parentItem->children())
    {
        QWidget *w = qobject_cast<QWidget*>(o);
        if (w)
        {
            QMenu *m = qobject_cast<QMenu*>(w);
            if (m)
            {
                if (s_menuHash.contains(m))
                {
                    if (s_menuHash.value(m) != w)
                    {
                        continue;
                    }
                }
                else
                {
                    s_menuHash.insert(m, w);
                }
                if (!m->isActiveWindow()) {
                    continue;
                }
            }
        }
        result.append(o);
    }
    return result;
}

QObject* WidgetsEnginePlatform::getParent(QObject* item)
{
    if (!item)
    {
        return nullptr;
    }
    QWidget* w = qobject_cast<QWidget*>(item);
    if (w)
    {
        return w->parentWidget();
    }
    return item->parent();
}

QWidget* WidgetsEnginePlatform::getItem(const QString& elementId)
{
    return qobject_cast<QWidget*>(getObject(elementId));
}

QWidget *WidgetsEnginePlatform::rootWidget()
{
    return qobject_cast<QWidget*>(rootObject());
}

QPointF WidgetsEnginePlatform::mapToGlobal(const QPointF &point)
{
    return QPointF(m_rootWidget->mapToGlobal(point.toPoint()));
}


void WidgetsEnginePlatform::removeItem(QObject* o)
{
    GenericEnginePlatform::removeItem(o);

    {
        QHash<QAction*, QSet<QWidget*> >::iterator i = s_actionHash.begin();
        while (i != s_actionHash.end())
        {
            if (i.value().contains(reinterpret_cast<QWidget*>(o))) {
                i.value().remove(reinterpret_cast<QWidget*>(o));
            }
            if (i.key() == o)
            {
                i = s_actionHash.erase(i);
                break;
            }
            else
            {
                ++i;
            }
        }
    }

    {
        QHash<QMenu*, QWidget*>::iterator i = s_menuHash.begin();
        while (i != s_menuHash.end())
        {
            if (i.value() == o || i.key() == o)
            {
                i = s_menuHash.erase(i);
                break;
            }
            else
            {
                ++i;
            }
        }
    }
}

QObject *WidgetsEnginePlatform::rootObject()
{
    if (qApp->activePopupWidget())
    {
        return qApp->activePopupWidget();
    }
    else if (qApp->activeModalWidget())
    {
        return qApp->activeModalWidget();
    }
    else
    {
        return m_rootObject;
    }
}

WidgetsEnginePlatform::WidgetsEnginePlatform(QWindow* window)
    : GenericEnginePlatform(window)
{
}

void WidgetsEnginePlatform::initialize()
{
    if (!m_rootWindow)
    {
        return;
    }

    if (!m_rootWindow->isActive())
    {
        m_rootWindow->raise();
        m_rootWindow->requestActivate();
        QTimer::singleShot(0, this, &WidgetsEnginePlatform::initialize);
        return;
    }

    if (qApp->activePopupWidget() && qApp->activePopupWidget()->windowHandle() == m_rootWindow)
    {
        m_rootWidget = qApp->activePopupWidget();
    }
    else if (qApp->activeModalWidget() && qApp->activeModalWidget()->windowHandle() == m_rootWindow)
    {
        m_rootWidget = qApp->activeModalWidget();
    }
    else if (qApp->focusWidget() && qApp->focusWidget()->windowHandle() == m_rootWindow)
    {
        m_rootWidget = qApp->focusWidget();
    }
    else
    {
        m_rootWidget = qApp->activeWindow();
    }

    m_mainWindow = qobject_cast<QMainWindow*>(m_rootWidget);

    m_rootObject = m_rootWidget;
    qCDebug(categoryWidgetsEnginePlatform) << Q_FUNC_INFO << m_rootObject;

    emit ready();
}

void WidgetsEnginePlatform::executeCommand_app_dumpInView(ITransportClient* socket,
                                                          const QString& elementId)
{
    qCDebug(categoryWidgetsEnginePlatform) << Q_FUNC_INFO << socket << elementId;

    QWidget* item = getItem(elementId);
    if (!item)
    {
        socketReply(socket, QString(), 1);
        return;
    }

    QAbstractItemView* view = qobject_cast<QAbstractItemView*>(item);
    if (!view)
    {
        socketReply(socket, QString(), 1);
        return;
    }

    QAbstractItemModel* model = view->model();
    if (!model)
    {
        socketReply(socket, QString(), 1);
        return;
    }

    qCDebug(categoryWidgetsEnginePlatform) << Q_FUNC_INFO << socket << view << model;

    socketReply(socket, recursiveDumpModel(model, QModelIndex()));
}

void WidgetsEnginePlatform::executeCommand_app_posInView(ITransportClient* socket,
                                                         const QString& elementId,
                                                         const QString& display)
{
    qCDebug(categoryWidgetsEnginePlatform) << Q_FUNC_INFO << socket << elementId << display;

    QWidget* item = getItem(elementId);
    if (!item)
    {
        socketReply(socket, QString(), 1);
        return;
    }

    QAbstractItemView* view = qobject_cast<QAbstractItemView*>(item);
    if (!view)
    {
        socketReply(socket, QString(), 1);
        return;
    }

    QAbstractItemModel* model = view->model();
    if (!model)
    {
        socketReply(socket, QString(), 1);
        return;
    }

    QModelIndex index = recursiveFindModel(model, QModelIndex(), display, true);
    QRect rect = view->visualRect(index);

    const QPoint itemPos = getAbsPosition(item);
    const QPoint indexCenter(rect.center().x() + itemPos.x(), rect.center().y() + itemPos.y());

    socketReply(socket,
                QStringList({QString::number(indexCenter.x()), QString::number(indexCenter.y())}));
}

void WidgetsEnginePlatform::executeCommand_app_clickInView(ITransportClient* socket,
                                                           const QString& elementId,
                                                           const QString& display)
{
    qCDebug(categoryWidgetsEnginePlatform) << Q_FUNC_INFO << socket << elementId << display;

    QWidget* item = getItem(elementId);
    if (!item)
    {
        socketReply(socket, QString(), 1);
        return;
    }

    QAbstractItemView* view = qobject_cast<QAbstractItemView*>(item);
    if (!view)
    {
        socketReply(socket, QString(), 1);
        return;
    }

    QAbstractItemModel* model = view->model();
    if (!model)
    {
        socketReply(socket, QString(), 1);
        return;
    }

    QModelIndex index = recursiveFindModel(model, QModelIndex(), display, true);
    QRect rect = view->visualRect(index);

    const QPoint itemPos = getAbsPosition(item);
    const QPoint indexCenter(rect.center().x() + itemPos.x(), rect.center().y() + itemPos.y());
    clickPoint(indexCenter);

    socketReply(socket,
                QStringList({QString::number(indexCenter.x()), QString::number(indexCenter.y())}));
}

void WidgetsEnginePlatform::executeCommand_app_scrollInView(ITransportClient* socket,
                                                            const QString& elementId,
                                                            const QString& display)
{
    qCDebug(categoryWidgetsEnginePlatform) << Q_FUNC_INFO << socket << elementId << display;

    QWidget* item = getItem(elementId);
    if (!item)
    {
        socketReply(socket, QString(), 1);
        return;
    }

    QAbstractItemView* view = qobject_cast<QAbstractItemView*>(item);
    if (!view)
    {
        socketReply(socket, QString(), 1);
        return;
    }

    QAbstractItemModel* model = view->model();
    if (!model)
    {
        socketReply(socket, QString(), 1);
        return;
    }

    QModelIndex index = recursiveFindModel(model, QModelIndex(), display, true);

    QItemSelectionModel* selectionModel = view->selectionModel();
    if (selectionModel)
    {
        selectionModel->select(index, QItemSelectionModel::ClearAndSelect);
    }

    view->scrollTo(index);
    view->setCurrentIndex(index);

    socketReply(socket, QString());
}

void WidgetsEnginePlatform::executeCommand_app_triggerInMenu(ITransportClient* socket,
                                                             const QString& text)
{
    qCDebug(categoryWidgetsEnginePlatform) << Q_FUNC_INFO << socket << text;

    for (QAction* a : s_actions)
    {
        if (a->text().contains(text) || a->shortcut().toString() == text)
        {
            QTimer::singleShot(0, a, &QAction::trigger);
            socketReply(socket, a->text());
            return;
        }
    }

    socketReply(socket, QString());
}

void WidgetsEnginePlatform::executeCommand_app_dumpInMenu(ITransportClient* socket)
{
    qCDebug(categoryWidgetsEnginePlatform) << Q_FUNC_INFO << socket;

    QStringList actions;

    for (QAction* a : s_actions)
    {
        actions.append(a->text());
    }

    socketReply(socket, actions);
}

void WidgetsEnginePlatform::executeCommand_app_dumpInComboBox(ITransportClient* socket,
                                                              const QString& elementId)
{
    qCDebug(categoryWidgetsEnginePlatform) << Q_FUNC_INFO << socket << elementId;

    QWidget* item = getItem(elementId);
    if (!item)
    {
        socketReply(socket, QString(), 1);
        return;
    }

    QComboBox* comboBox = qobject_cast<QComboBox*>(item);
    if (!comboBox)
    {
        socketReply(socket, QString(), 1);
        return;
    }

    QAbstractItemModel* model = comboBox->model();
    if (!model)
    {
        socketReply(socket, QString(), 1);
        return;
    }

    qCDebug(categoryWidgetsEnginePlatform) << Q_FUNC_INFO << socket << comboBox << model;

    socketReply(socket, recursiveDumpModel(model, QModelIndex()));
}

void WidgetsEnginePlatform::executeCommand_app_activateInComboBox(ITransportClient* socket,
                                                                  const QString& elementId,
                                                                  const QString& display)
{
    qCDebug(categoryWidgetsEnginePlatform) << Q_FUNC_INFO << socket << elementId << display;

    QWidget* item = getItem(elementId);
    if (!item)
    {
        socketReply(socket, QString(), 1);
        return;
    }

    QComboBox* comboBox = qobject_cast<QComboBox*>(item);
    if (!comboBox)
    {
        socketReply(socket, QString(), 1);
        return;
    }

    int index = comboBox->findText(display);
    if (index < 0)
    {
        socketReply(socket, QString(), 1);
    }

    comboBox->setCurrentIndex(index);

    socketReply(socket, QString());
}

void WidgetsEnginePlatform::executeCommand_app_activateInComboBox(ITransportClient* socket,
                                                                  const QString& elementId,
                                                                  qlonglong idx)
{
    const int index = idx;

    qCDebug(categoryWidgetsEnginePlatform) << Q_FUNC_INFO << socket << elementId << index;

    QWidget* item = getItem(elementId);
    if (!item)
    {
        socketReply(socket, QString(), 1);
        return;
    }

    QComboBox* comboBox = qobject_cast<QComboBox*>(item);
    if (!comboBox)
    {
        socketReply(socket, QString(), 1);
        return;
    }

    if (index >= comboBox->count())
    {
        socketReply(socket, QString(), 1);
    }

    comboBox->setCurrentIndex(index);

    socketReply(socket, QString());
}

void WidgetsEnginePlatform::executeCommand_app_dumpInTabBar(ITransportClient* socket,
                                                            const QString& elementId)
{
    qCDebug(categoryWidgetsEnginePlatform) << Q_FUNC_INFO << socket << elementId;

    QWidget* item = getItem(elementId);
    if (!item)
    {
        socketReply(socket, QString(), 1);
        return;
    }

    QTabBar* tabBar = qobject_cast<QTabBar*>(item);
    if (!tabBar)
    {
        socketReply(socket, QString(), 1);
        return;
    }

    QStringList tabs;

    for (int i = 0; i < tabBar->count(); i++)
    {
        tabs.append(tabBar->tabText(i));
    }

    socketReply(socket, tabs);
}

void WidgetsEnginePlatform::executeCommand_app_posInTabBar(ITransportClient* socket,
                                                           const QString& elementId,
                                                           const QString& display)
{
    qCDebug(categoryWidgetsEnginePlatform) << Q_FUNC_INFO << socket << elementId << display;

    QWidget* item = getItem(elementId);
    if (!item)
    {
        socketReply(socket, QString(), 1);
        return;
    }

    QTabBar* tabBar = qobject_cast<QTabBar*>(item);
    if (!tabBar)
    {
        socketReply(socket, QString(), 1);
        return;
    }

    for (int i = 0; i < tabBar->count(); i++)
    {
        if (tabBar->tabText(i) == display)
        {
            QRect rect = tabBar->tabRect(i);

            const QPoint itemPos = getAbsPosition(item);
            const QPoint indexCenter(rect.center().x() + itemPos.x(),
                                     rect.center().y() + itemPos.y());

            socketReply(
                socket,
                QStringList({QString::number(indexCenter.x()), QString::number(indexCenter.y())}));
            return;
        }
    }

    socketReply(socket, QString(), 11);
}

void WidgetsEnginePlatform::executeCommand_app_posInTabBar(ITransportClient* socket,
                                                           const QString& elementId,
                                                           qlonglong idx)
{
    const int index = idx;

    qCDebug(categoryWidgetsEnginePlatform) << Q_FUNC_INFO << socket << elementId << index;

    QWidget* item = getItem(elementId);
    if (!item)
    {
        socketReply(socket, QString(), 1);
        return;
    }

    QTabBar* tabBar = qobject_cast<QTabBar*>(item);
    if (!tabBar)
    {
        socketReply(socket, QString(), 1);
        return;
    }

    if (index >= tabBar->count())
    {
        socketReply(socket, QString(), 1);
        return;
    }

    QRect rect = tabBar->tabRect(index);

    const QPoint itemPos = getAbsPosition(item);
    const QPoint indexCenter(rect.center().x() + itemPos.x(), rect.center().y() + itemPos.y());

    socketReply(socket,
                QStringList({QString::number(indexCenter.x()), QString::number(indexCenter.y())}));
}

void WidgetsEnginePlatform::executeCommand_app_activateInTabBar(ITransportClient* socket,
                                                                const QString& elementId,
                                                                const QString& display)
{
    qCDebug(categoryWidgetsEnginePlatform) << Q_FUNC_INFO << socket << elementId << display;

    QWidget* item = getItem(elementId);
    if (!item)
    {
        socketReply(socket, QString(), 1);
        return;
    }

    QTabBar* tabBar = qobject_cast<QTabBar*>(item);
    if (!tabBar)
    {
        socketReply(socket, QString(), 1);
        return;
    }

    for (int i = 0; i < tabBar->count(); i++)
    {
        if (tabBar->tabText(i) == display)
        {
            tabBar->setCurrentIndex(i);
            socketReply(socket, QString());
            return;
        }
    }

    socketReply(socket, QString(), 1);
}

void WidgetsEnginePlatform::executeCommand_app_activateInTabBar(ITransportClient* socket,
                                                                const QString& elementId,
                                                                qlonglong idx)
{
    const int index = idx;

    qCDebug(categoryWidgetsEnginePlatform) << Q_FUNC_INFO << socket << elementId << index;

    QWidget* item = getItem(elementId);
    if (!item)
    {
        socketReply(socket, QString(), 1);
        return;
    }

    QTabBar* tabBar = qobject_cast<QTabBar*>(item);
    if (!tabBar)
    {
        socketReply(socket, QString(), 1);
        return;
    }

    if (index >= tabBar->count())
    {
        socketReply(socket, QString(), 1);
        return;
    }

    tabBar->setCurrentIndex(index);
    socketReply(socket, QString());
}

QModelIndex WidgetsEnginePlatform::recursiveFindModel(QAbstractItemModel* model,
                                                      QModelIndex index,
                                                      const QString& display,
                                                      bool partial)
{
    for (int r = 0; r < model->rowCount(index); r++)
    {
        for (int c = 0; c < model->columnCount(index); c++)
        {
            if (model->hasIndex(r, c, index))
            {
                QModelIndex newIndex = model->index(r, c, index);
                const QString text = model->data(newIndex).toString();
                if ((partial && text.contains(display)) || text == display)
                {
                    return newIndex;
                }
                QModelIndex findIndex = recursiveFindModel(model, newIndex, display, partial);
                if (findIndex.isValid())
                {
                    return findIndex;
                }
            }
        }
    }
    return QModelIndex();
}

QStringList WidgetsEnginePlatform::recursiveDumpModel(QAbstractItemModel* model, QModelIndex index)
{
    QStringList results;
    for (int r = 0; r < model->rowCount(index); r++)
    {
        for (int c = 0; c < model->columnCount(index); c++)
        {
            if (model->hasIndex(r, c, index))
            {
                QModelIndex newIndex = model->index(r, c, index);
                results.append(model->data(newIndex).toString());
                results.append(recursiveDumpModel(model, newIndex));
            }
        }
    }
    return results;
}

QRect WidgetsEnginePlatform::getActionGeometry(QAction* action)
{
    if (m_mainWindow && m_mainWindow->menuBar() &&
        m_mainWindow->menuBar()->actions().contains(action))
    {
        QRect geometry = m_mainWindow->menuBar()->actionGeometry(action);
        geometry.translate(m_mainWindow->menuBar()->pos());
        return geometry;
    }
    if (s_actionHash.contains(action))
    {
        for (QWidget* w : s_actionHash.value(action))
        {
            if (w->isVisible())
            {
                QMenu* m = qobject_cast<QMenu*>(w);
                if (m)
                {
                    QRect geometry = m->actionGeometry(action);
                    geometry.translate(m_rootWidget->mapFromGlobal(m->pos()));
                    return geometry;
                }
            }
        }
    }
    return {};
}

QPoint WidgetsEnginePlatform::getAbsPosition(QObject* item)
{
    QAction* a = qobject_cast<QAction*>(item);
    if (a)
    {
        auto actionGeometry = getActionGeometry(a);
        if (actionGeometry.isValid())
        {
            return actionGeometry.topLeft();
        }
    }
    QWidget* w = qobject_cast<QWidget*>(item);
    if (w)
    {
        if (w == m_rootWidget)
        {
            return QPoint();
        }
        if (auto* popup = qApp->activePopupWidget())
        {
            QPoint pos = m_rootWidget->mapFromGlobal(popup->pos());
            if (w != popup && w->isVisible())
            {
                pos += w->mapTo(popup, QPoint(0, 0));
            }
            return pos;
        }
        return w->mapTo(m_rootWidget, QPoint(0, 0));
    }
    QGraphicsScene *gs = qobject_cast<QGraphicsScene*>(item);
    if (gs)
    {
        return getAbsPosition(item->parent()) + gs->sceneRect().topLeft().toPoint();
    }
    QGraphicsObject *go = qobject_cast<QGraphicsObject*>(item);
    if (go)
    {
        return getAbsPosition(go->scene()) + go->mapToScene(QPointF()).toPoint();
    }
    return QPoint();
}

QPoint WidgetsEnginePlatform::getPosition(QObject* item)
{
    QWidget* w = qobject_cast<QWidget*>(item);
    if (w)
    {
        return w->pos();
    }
    return QPoint();
}

QSize WidgetsEnginePlatform::getSize(QObject* item)
{
    QAction* a = qobject_cast<QAction*>(item);
    if (a)
    {
        auto actionGeometry = getActionGeometry(a);
        if (!actionGeometry.isEmpty())
        {
            return actionGeometry.size();
        }
    }
    QWidget* w = qobject_cast<QWidget*>(item);
    if (w)
    {
        return w->size();
    }
    QGraphicsScene *gs = qobject_cast<QGraphicsScene*>(item);
    if (gs)
    {
        return gs->sceneRect().size().toSize();
    }
    QGraphicsObject *go = qobject_cast<QGraphicsObject*>(item);
    if (go)
    {
        QGraphicsWidget *gw = qobject_cast<QGraphicsWidget*>(go);
        if (gw)
        {
            return gw->geometry().size().toSize();
        }
    }
    return QSize();
}

bool WidgetsEnginePlatform::isItemEnabled(QObject* item)
{
    QWidget* w = qobject_cast<QWidget*>(item);
    if (w)
    {
        return w->isEnabled();
    }
    QGraphicsObject *go = qobject_cast<QGraphicsObject*>(item);
    if (go)
    {
        return go->isEnabled();
    }
    return false;
}

bool WidgetsEnginePlatform::isItemVisible(QObject* item)
{
    QWidget* w = qobject_cast<QWidget*>(item);
    if (w)
    {
        return w->isVisible();
    }
    QGraphicsObject *go = qobject_cast<QGraphicsObject*>(item);
    if (go)
    {
        return go->isVisible();
    }
    return false;
}

qreal WidgetsEnginePlatform::itemOpacity(QObject *)
{
    return 1.0;
}

void WidgetsEnginePlatform::grabScreenshot(ITransportClient* socket,
                                           QObject* item,
                                           bool fillBackground)
{
    qCDebug(categoryWidgetsEnginePlatform) << Q_FUNC_INFO << socket << item << fillBackground;

    QWidget* w = qobject_cast<QWidget*>(item);
    if (!w)
    {
        return;
    }

    QByteArray arr;
    QBuffer buffer(&arr);
    QPixmap pix;

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    if (w == m_rootWidget)
    {
        pix = w->screen()->grabWindow(
            0, m_rootWindow->x(), m_rootWindow->y(), m_rootWindow->width(), m_rootWindow->height());
    }
    else
#endif
    {
        pix = w->grab();
    }

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

void WidgetsEnginePlatform::pressAndHoldItem(QObject* qitem, int delay)
{
    qCDebug(categoryWidgetsEnginePlatform) << Q_FUNC_INFO << qitem << delay;

    if (!qitem)
    {
        return;
    }

    QWidget* item = qobject_cast<QWidget*>(qitem);
    if (!item)
    {
        return;
    }

    const QPoint itemCenter = getAbsGeometry(item).center();
    pressAndHold(itemCenter.x(), itemCenter.y(), delay);
}

EventHandler::EventHandler(QObject* parent)
    : QObject(parent)
{
}

bool EventHandler::eventFilter(QObject* watched, QEvent* event)
{
    switch (event->type())
    {
        case QEvent::ActionAdded:
        {
            QActionEvent* ae = static_cast<QActionEvent*>(event);
            s_actions.append(ae->action());
            break;
        }
        case QEvent::ActionRemoved:
        {
            QActionEvent* ae = static_cast<QActionEvent*>(event);
            s_actions.removeAll(ae->action());
            break;
        }
        default:
            break;
    }

    return QObject::eventFilter(watched, event);
}
