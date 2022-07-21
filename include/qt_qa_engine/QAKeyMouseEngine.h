// Copyright (c) 2019-2020 Open Mobile Platform LLC.
#ifndef QAKEYMOUSEENGINE_H
#define QAKEYMOUSEENGINE_H

#include <QObject>
#include <QPointF>
#include <QTouchEvent>
#include <QVariant>

class QAPendingEvent;
class QElapsedTimer;
class QTimer;
class QTouchDevice;
class QMouseEvent;
class QKeyEvent;
class QAKeyMouseEngine : public QObject
{
    Q_OBJECT
public:
    explicit QAKeyMouseEngine(QObject* parent = nullptr);
    bool isRunning() const;

    enum MouseMode
    {
        MouseEventMode,
        TouchEventMode,
    };
    MouseMode mode();
    void setMode(MouseMode mode);

    QAPendingEvent* click(const QPointF& point);
    QAPendingEvent* pressAndHold(const QPointF& point, int delay = 1200);
    QAPendingEvent* drag(const QPointF& pointA,
                         const QPointF& pointB,
                         int delay = 1200,
                         int duration = 500,
                         int moveSteps = 20,
                         int releaseDelay = 600);
    QAPendingEvent* move(const QPointF& pointA,
                         const QPointF& pointB,
                         int duration = 500,
                         int moveSteps = 20,
                         int releaseDelay = 600);

    void pressEnter();

    QAPendingEvent* performMultiAction(const QVariantList& multiActions);
    QAPendingEvent* performTouchAction(const QVariantList& actions);

    QAPendingEvent* performChainActions(const QVariantList& actions);

    int getNextPointId();
    qint64 getEta();
    QTouchDevice* getTouchDevice();

signals:
    void touchEvent(const QTouchEvent& event);
    void mouseEvent(const QMouseEvent& event);
    void keyEvent(const QKeyEvent& event);

private slots:
    void onPressed(const QPointF point);
    void onMoved(const QPointF point);
    void onReleased(const QPointF point);

    void sendKeyPress(const QChar &text, int key = 0);
    void sendKeyRelease(const QChar &text, int key = 0);

    void onKeyPressed(const QString &value);
    void onKeyReleased(const QString &value);
    void onMousePressed(const QPointF point, int button = 0);
    void onMouseReleased(const QPointF point, int button = 0);
    void onMouseMoved(const QPointF point);

private:
    void handleKey(const QString &value, bool keyUp);

private:
    friend class TouchAction;
    QElapsedTimer* m_eta;

    QHash<QObject*, QTouchEvent::TouchPoint> m_touchPoints;

    MouseMode m_mode = MouseEventMode;

    QTouchDevice* m_touchDevice;
    int m_tpId = 0;

    Qt::KeyboardModifiers m_mods = Qt::NoModifier;
    Qt::MouseButtons m_buttons = Qt::NoButton;
    QPoint m_previousPoint;
};

class EventWorker : public QObject
{
    Q_OBJECT
public:
    explicit EventWorker(const QVariantList& actions);
    virtual ~EventWorker();
    static EventWorker* PerformTouchAction(const QVariantList& actions);
    static EventWorker* PerformChainAction(const QVariantList& actions);

public slots:
    void start();
    void startChain();

private:
    void sendPress(const QPointF& point);
    void sendRelease(const QPointF& point);
    void sendMove(const QPointF& point);
    void sendMove(const QPointF& previousPoint, const QPointF& point, int duration, int moveSteps);

    QList<QPointF> moveInterpolator(const QPointF& previousPoint, const QPointF& point, int moveSteps);

    QVariantList m_actions;

signals:
    void pressed(const QPointF point);
    void moved(const QPointF point);
    void released(const QPointF point);

    void mousePressed(const QPointF point, int button = 0);
    void mouseReleased(const QPointF point, int button = 0);
    void mouseMoved(const QPointF point);

    void keyPressed(const QString &value);
    void keyReleased(const QString &value);

    void finished();
};

#endif // QAKEYMOUSEENGINE_H
