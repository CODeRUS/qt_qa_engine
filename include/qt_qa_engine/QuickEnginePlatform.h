#pragma once

#include <qt_qa_engine/GenericEnginePlatform.h>

#include <QJsonObject>

class QQuickItem;
class QQmlEngine;
class QQuickWindow;
class QXmlStreamWriter;
class QuickEnginePlatform : public GenericEnginePlatform
{
    Q_OBJECT
public:
    explicit QuickEnginePlatform(QWindow* window);
    QQuickItem* getItem(const QString& elementId);

    QObject* getParent(QObject* item) override;
    QPoint getAbsPosition(QObject* item) override;
    QPoint getPosition(QObject* item) override;
    QSize getSize(QObject* item) override;
    bool isItemEnabled(QObject* item) override;
    bool isItemVisible(QObject* item) override;
    qreal itemOpacity(QObject *item) override;
    QPointF mapToGlobal(const QPointF &point) override;

    QString getObjectId(QObject *item) override;

public slots:
    virtual void initialize() override;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

    QList<QObject*> childrenList(QObject* parentItem) override;

    QQuickItem* findParentFlickable(QQuickItem* rootItem = nullptr);
    QVariantList findNestedFlickable(QQuickItem* parentItem = nullptr);
    QQuickItem* getApplicationWindow();

    QVariant executeJS(const QString& jsCode, QQuickItem* item);

    QByteArray grabDirectScreenshot() override;

    void grabScreenshot(ITransportClient* socket,
                        QObject* item,
                        bool fillBackground = false) override;

    void pressAndHoldItem(QObject* qitem, int delay = 800) override;
    void clearFocus();
    void clearComponentCache();

    QQmlEngine* getEngine(QQuickItem* item = nullptr);

    QQuickItem* m_rootQuickItem = nullptr;
    QQuickWindow* m_rootQuickWindow = nullptr;
    QQuickItem* m_touchIndicator = nullptr;

private slots:
    // execute_%1 methods
    void executeCommand_touch_pressAndHold(ITransportClient* socket,
                                           qlonglong posx,
                                           qlonglong posy);
    void executeCommand_touch_mouseSwipe(
        ITransportClient* socket, qlonglong posx, qlonglong posy, qlonglong stopx, qlonglong stopy);
    void executeCommand_touch_mouseDrag(
        ITransportClient* socket, qlonglong posx, qlonglong posy, qlonglong stopx, qlonglong stopy);
    void executeCommand_app_js(ITransportClient* socket,
                               const QString& elementId,
                               const QString& jsCode);
};
