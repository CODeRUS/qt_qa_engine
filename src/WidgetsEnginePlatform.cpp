// Copyright (c) 2019-2020 Open Mobile Platform LLC.
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

namespace
{

QList<QAction*> s_actions;
QHash<QAction*, QWidget*> s_actionHash;
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
    QWidget* widget = qobject_cast<QWidget*>(parentItem);
    if (widget)
    {
        for (QAction* action : widget->actions())
        {
            if (s_actionHash.contains(action))
            {
                if (s_actionHash.value(action) != widget)
                {
                    continue;
                }
            }
            else
            {
                s_actionHash.insert(action, widget);
            }
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
                result.append(action->menu());
            }
            result.append(action);
        }
    }
    for (QWidget* w : parentItem->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly))
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
        }
        result.append(w);
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
    if (!w)
    {
        return item->parent();
    }
    return w->parentWidget();
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
    if (s_actionHash.contains(action))
    {
        QWidget* w = s_actionHash.value(action);
        QMenu* m = qobject_cast<QMenu*>(w);
        if (m)
        {
            QRect geometry = m->actionGeometry(action);
            //geometry.translate(m_rootWidget->mapFromGlobal(m->pos()));
            geometry.translate(m_rootWidget->mapFromGlobal(m->pos()));
            return geometry;
        }
    }
    if (m_mainWindow && m_mainWindow->menuBar() &&
        m_mainWindow->menuBar()->actions().contains(action))
    {
        QRect geometry = m_mainWindow->menuBar()->actionGeometry(action);
        geometry.translate(m_mainWindow->menuBar()->pos());
        return geometry;
    }
    return {};
}

QPoint WidgetsEnginePlatform::getAbsPosition(QObject* item)
{
    QAction* a = qobject_cast<QAction*>(item);
    if (a)
    {
        auto actionGeometry = getActionGeometry(a);
        if (!actionGeometry.isEmpty())
        {
            return actionGeometry.topLeft();
        }
    }
    QWidget* w = qobject_cast<QWidget*>(item);
    if (!w)
    {
        return QPoint();
    }
    if (w == qApp->activePopupWidget())
    {
        return m_rootWidget->mapFromGlobal(w->pos());
    }
    else if (w == m_rootWidget)
    {
        return QPoint();
    }
    else if (qApp->activePopupWidget())
    {
        return m_rootWidget->mapFromGlobal(qApp->activePopupWidget()->mapToParent(w->pos()));
    }
    return w->mapTo(m_rootWidget, QPoint(0, 0));
}

QPoint WidgetsEnginePlatform::getClickPosition(QObject *item)
{
    auto pos = getAbsPosition(item);
    qDebug() << Q_FUNC_INFO << item << pos;
    if (qApp->activePopupWidget())
    {
        pos = m_rootWidget->mapToGlobal(pos);
    }
    qDebug() << Q_FUNC_INFO << item << pos;
    return pos;
}

QPoint WidgetsEnginePlatform::getPosition(QObject* item)
{
    QWidget* w = qobject_cast<QWidget*>(item);
    if (!w)
    {
        return QPoint();
    }
    return w->pos();
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
    if (!w)
    {
        return QSize();
    }
    return w->size();
}

bool WidgetsEnginePlatform::isItemEnabled(QObject* item)
{
    QWidget* w = qobject_cast<QWidget*>(item);
    if (!w)
    {
        return false;
    }
    return w->isEnabled();
}

bool WidgetsEnginePlatform::isItemVisible(QObject* item)
{
    QWidget* w = qobject_cast<QWidget*>(item);
    if (!w)
    {
        return false;
    }
    return w->isVisible();
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

    if (w == m_rootWidget)
    {
        pix = w->screen()->grabWindow(
            0, m_rootWindow->x(), m_rootWindow->y(), m_rootWindow->width(), m_rootWindow->height());
    }
    else
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

    const QPointF itemAbs = getAbsPosition(item);
    pressAndHold(itemAbs.x() + item->width() / 2, itemAbs.y() + item->height() / 2, delay);
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
