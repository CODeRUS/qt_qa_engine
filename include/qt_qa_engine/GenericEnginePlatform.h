#pragma once

#include <qt_qa_engine/IEnginePlatform.h>

class QAKeyMouseEngine;
class QTouchEvent;
class QMouseEvent;
class QKeyEvent;
class QWindow;
class QXmlStreamWriter;

class AnalyzeEventFilter : public QObject
{
    Q_OBJECT

public:
    explicit AnalyzeEventFilter(QObject* parent = nullptr);

signals:
    void pressed(const QPoint &point);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
};

class GenericEnginePlatform : public IEnginePlatform
{
    Q_OBJECT
public:
    explicit GenericEnginePlatform(QWindow* window);
    QWindow* window() override;
    QObject* rootObject() override;

    void socketReply(ITransportClient* socket, const QVariant& value, int status = 0) override;
    void elementReply(ITransportClient* socket,
                      QObjectList elements,
                      bool multiple = false) override;

    void addItem(QObject* o) override;
    void removeItem(QObject* o) override;

    static QString getClassName(QObject* item);
    static QString uniqueId(QObject* item);

    bool containsObject(const QString& elementId) override;
    QObject* getObject(const QString& elementId) override;
    QString getText(QObject* item) override;
    QRect getGeometry(QObject* item) override;
    QRect getAbsGeometry(QObject* item) override;
    QPoint getClickPosition(QObject *item) override;

    void activateWindow() override;

    virtual QString getObjectId(QObject *item) override;

protected:
    friend class QAKeyMouseEngine;

    void findElement(ITransportClient* socket,
                     const QString& strategy,
                     const QString& selector,
                     bool multiple = false,
                     QObject* item = nullptr);
    void findByProperty(ITransportClient* socket,
                        const QString& propertyName,
                        const QVariant& propertyValue,
                        bool multiple = false,
                        QObject* parentItem = nullptr);
    void setProperty(ITransportClient* socket,
                     const QString& property,
                     const QVariant& value,
                     const QString& elementId);

    virtual QList<QObject*> childrenList(QObject* parentItem) = 0;
    QObject* findItemById(const QString& id, QObject* parentItem = nullptr);
    QObjectList findItemsByObjectName(const QString& objectName,
                                      QObject* parentItem = nullptr,
                                      bool multiple = true);
    QObjectList findItemsByObjectId(const QString& objectId,
                                    QObject* parentItem = nullptr,
                                    bool multiple = true);
    QObjectList findItemsByClassName(const QString& className,
                                     QObject* parentItem = nullptr,
                                     bool multiple= true);
    QObjectList findItemsByProperty(const QString& propertyName,
                                    const QVariant& propertyValue,
                                    QObject* parentItem = nullptr,
                                    bool multiple = true);
    QObjectList findItemsByText(const QString& text,
                                bool partial = true,
                                QObject* parentItem = nullptr,
                                bool multiple = true);
    QObjectList findItemsByXpath(const QString& xpath, QObject* parentItem = nullptr);
    QObjectList filterVisibleItems(QObjectList items);

    QJsonObject dumpObject(QObject* item, const QVariantList &filters, int depth = 0);
    QJsonObject recursiveDumpTree(QObject* rootItem, const QVariantList &filters, int depth = 0);
    bool recursiveDumpXml(QXmlStreamWriter* writer, QObject* rootItem, int depth = 0);

    virtual QByteArray grabDirectScreenshot() = 0;

    virtual void grabScreenshot(ITransportClient* socket,
                                QObject* item,
                                bool fillBackground = false) = 0;
    void waitForClick(ITransportClient* socket, QObject*);
    void clickItem(QObject* item);

    void clickPoint(float posx, float posy);
    void clickPoint(const QPoint& pos);
    virtual void pressAndHoldItem(QObject* item, int delay = 800) = 0;
    void pressAndHold(float posx, float posy, int delay = 800);
    void mouseMove(float startx, float starty, float stopx, float stopy);
    void mouseDrag(float startx, float starty, float stopx, float stopy, int delay = 1200);
    void processTouchActionList(const QVariant& actionListArg);
    bool waitForPropertyChange(QObject* item,
                               const QString& propertyName,
                               const QVariant& value,
                               int timeout = 10000);
    bool waitForWindowChange(int timeout = 10000);
    bool registerSignal(QObject* item,
                        const QString& signalName);
    bool unregisterSignal(QObject* item,
                        const QString& signalName);

    bool checkMatch(const QString& pattern, const QString& value);

    QWindow* m_rootWindow = nullptr;
    QObject* m_rootObject = nullptr;

    QHash<QString, QObject*> m_items;
    QAKeyMouseEngine* m_keyMouseEngine = nullptr;

    QHash<QString, QStringList> m_blacklistedProperties;
    QHash<QString, int> m_signalCounter;

    QVariantList m_lastFilters;
    ITransportClient* m_analyzeSocket = nullptr;
    AnalyzeEventFilter* m_analyzeEventFilter = nullptr;

public slots:
    virtual void focusWindowChanged(QWindow *w) override;

signals:
    void ready();
    void focusLost();
    void focusRestored();

private:
    void execute(ITransportClient* socket, const QString& methodName, const QVariantList& params);

private slots:
    // own stuff
    void onPropertyChanged();
    void onSignalReceived();

    void analyzePressed(const QPoint &point);

    // synthesized input events
    virtual void onTouchEvent(const QTouchEvent& event);
    virtual void onMouseEvent(const QMouseEvent& event);
    virtual void onKeyEvent(const QKeyEvent& event);
    virtual void onWheelEvent(const QWheelEvent& event);

    // IEnginePlatform interface
    virtual void appConnectCommand(ITransportClient* socket) override;
    virtual void appDisconnectCommand(ITransportClient* socket) override;

    virtual void initializeCommand(ITransportClient* socket) override;
    virtual void activateAppCommand(ITransportClient* socket, const QString& appName) override;
    virtual void closeAppCommand(ITransportClient* socket, const QString& appName) override;
    virtual void closeWindowCommand(ITransportClient* socket) override;
    virtual void queryAppStateCommand(ITransportClient* socket, const QString& appName) override;
    virtual void backgroundCommand(ITransportClient* socket, qlonglong seconds) override;
    virtual void getClipboardCommand(ITransportClient* socket) override;
    virtual void setClipboardCommand(ITransportClient* socket, const QString& content) override;
    virtual void findElementCommand(ITransportClient* socket,
                                    const QString& strategy,
                                    const QString& selector) override;
    virtual void findElementsCommand(ITransportClient* socket,
                                     const QString& strategy,
                                     const QString& selector) override;
    virtual void findElementFromElementCommand(ITransportClient* socket,
                                               const QString& strategy,
                                               const QString& selector,
                                               const QString& elementId) override;
    virtual void findElementsFromElementCommand(ITransportClient* socket,
                                                const QString& strategy,
                                                const QString& selector,
                                                const QString& elementId) override;
    virtual void getLocationCommand(ITransportClient* socket, const QString& elementId) override;
    virtual void getLocationInViewCommand(ITransportClient* socket,
                                          const QString& elementId) override;
    virtual void getAttributeCommand(ITransportClient* socket,
                                     const QString& attribute,
                                     const QString& elementId) override;
    virtual void getPropertyCommand(ITransportClient* socket,
                                    const QString& attribute,
                                    const QString& elementId) override;
    virtual void getTextCommand(ITransportClient* socket, const QString& elementId) override;
    virtual void getElementScreenshotCommand(ITransportClient* socket,
                                             const QString& elementId) override;
    virtual void getScreenshotCommand(ITransportClient* socket) override;
    virtual void getWindowRectCommand(ITransportClient* socket) override;
    virtual void elementEnabledCommand(ITransportClient* socket, const QString& elementId) override;
    virtual void elementDisplayedCommand(ITransportClient* socket,
                                         const QString& elementId) override;
    virtual void elementSelectedCommand(ITransportClient* socket,
                                        const QString& elementId) override;
    virtual void getSizeCommand(ITransportClient* socket, const QString& elementId) override;
    virtual void getElementRectCommand(ITransportClient* socket, const QString& elementId) override;
    virtual void setValueImmediateCommand(ITransportClient* socket,
                                          const QVariantList& value,
                                          const QString& elementId) override;
    virtual void replaceValueCommand(ITransportClient* socket,
                                     const QVariantList& value,
                                     const QString& elementId) override;
    virtual void setValueCommand(ITransportClient* socket,
                                 const QVariantList& value,
                                 const QString& elementId) override;
    virtual void setValueCommand(ITransportClient* socket,
                                 const QString& value,
                                 const QString& elementId) override;
    virtual void clickCommand(ITransportClient* socket, const QString& elementId) override;
    virtual void clearCommand(ITransportClient* socket, const QString& elementId) override;
    virtual void submitCommand(ITransportClient* socket, const QString& elementId) override;
    virtual void getPageSourceCommand(ITransportClient* socket) override;
    virtual void backCommand(ITransportClient* socket) override;
    virtual void forwardCommand(ITransportClient* socket) override;
    virtual void getOrientationCommand(ITransportClient* socket) override;
    virtual void setOrientationCommand(ITransportClient* socket,
                                       const QString& orientation) override;
    virtual void hideKeyboardCommand(ITransportClient* socket,
                                     const QString& strategy,
                                     const QString& key,
                                     qlonglong keyCode,
                                     const QString& keyName) override;
    virtual void getCurrentActivityCommand(ITransportClient* socket) override;
    virtual void implicitWaitCommand(ITransportClient* socket, qlonglong msecs) override;
    virtual void activeCommand(ITransportClient* socket) override;
    virtual void getAlertTextCommand(ITransportClient* socket) override;
    virtual void isKeyboardShownCommand(ITransportClient* socket) override;
    virtual void activateIMEEngineCommand(ITransportClient* socket,
                                          const QVariant& engine) override;
    virtual void availableIMEEnginesCommand(ITransportClient* socket) override;
    virtual void getActiveIMEEngineCommand(ITransportClient* socket) override;
    virtual void deactivateIMEEngineCommand(ITransportClient* socket) override;
    virtual void isIMEActivatedCommand(ITransportClient* socket) override;
    virtual void keyeventCommand(ITransportClient* socket,
                                 const QVariant& keycodeArg,
                                 const QVariant& metaState,
                                 const QVariant& sessionIDArg,
                                 const QVariant& flagsArg) override;
    virtual void longPressKeyCodeCommand(ITransportClient* socket,
                                         const QVariant& keycodeArg,
                                         const QVariant& metaState,
                                         const QVariant& flagsArg) override;
    virtual void pressKeyCodeCommand(ITransportClient* socket,
                                     const QVariant& keycodeArg,
                                     const QVariant& metaState,
                                     const QVariant& flagsArg) override;
    virtual void executeCommand(ITransportClient* socket,
                                const QString& command,
                                const QVariantList& params) override;
    virtual void executeAsyncCommand(ITransportClient* socket,
                                     const QString& command,
                                     const QVariantList& params) override;
    virtual void performTouchCommand(ITransportClient* socket, const QVariant& paramsArg) override;
    virtual void performMultiActionCommand(ITransportClient* socket,
                                           const QVariant& paramsArg) override;
    virtual void performActionsCommand(ITransportClient* socket,
                                       const QVariant& paramsArg) override;

    virtual void getTimeoutsCommand(ITransportClient* socket) override;

    virtual void startAnalyzeCommand(ITransportClient* socket) override;
    virtual void stopAnalyzeCommand(ITransportClient* socket) override;

    // findElement_%1 methods
    void findStrategy_id(ITransportClient* socket,
                         const QString& selector,
                         bool multiple = false,
                         QObject* parentItem = nullptr);
    void findStrategy_objectname(ITransportClient* socket,
                                 const QString& selector,
                                 bool multiple = false,
                                 QObject* parentItem = nullptr);
    void findStrategy_objectid(ITransportClient* socket,
                                 const QString& selector,
                                 bool multiple = false,
                                 QObject* parentItem = nullptr);
    void findStrategy_classname(ITransportClient* socket,
                                const QString& selector,
                                bool multiple = false,
                                QObject* parentItem = nullptr);
    void findStrategy_name(ITransportClient* socket,
                           const QString& selector,
                           bool multiple = false,
                           QObject* parentItem = nullptr);
    void findStrategy_parent(ITransportClient* socket,
                             const QString& selector,
                             bool multiple = false,
                             QObject* parentItem = nullptr);
    void findStrategy_xpath(ITransportClient* socket,
                            const QString& selector,
                            bool multiple = false,
                            QObject* parentItem = nullptr);

    // execute_%1 methods
    void executeCommand_activateApp(ITransportClient* socket, const QVariant& appName);
    void executeCommand_submit(ITransportClient* socket, const QVariant& element);

    void executeCommand_window_frameSize(ITransportClient* socket);
    void executeCommand_app_method(ITransportClient* socket,
                                   const QString& elementId,
                                   const QString& method,
                                   const QVariantList& params);
    void executeCommand_app_dumpTree(ITransportClient* socket);
    void executeCommand_app_dumpTreeFilter(ITransportClient* socket, const QVariantList &filters);
    void executeCommand_app_setAttribute(ITransportClient* socket,
                                         const QString& elementId,
                                         const QString& attribute,
                                         const QVariant& value);
    void executeCommand_app_waitForPropertyChange(ITransportClient* socket,
                                                  const QString& elementId,
                                                  const QString& propertyName,
                                                  const QVariant& value,
                                                  qlonglong timeout = 3000);
    void executeCommand_app_waitForWindowChange(ITransportClient* socket,
                                                qlonglong timeout = 3000);
    void executeCommand_app_registerSignal(ITransportClient* socket,
                                                const QString& elementId,
                                                const QString& signalName);
    void executeCommand_app_unregisterSignal(ITransportClient* socket,
                                                  const QString& elementId,
                                                  const QString& signalName);
    void executeCommand_app_countSignals(ITransportClient* socket,
                                              const QString& elementId,
                                              const QString& signalName);
    void executeCommand_app_setLoggingFilter(ITransportClient* socket, const QString& rules);
    void executeCommand_app_installFileLogger(ITransportClient* socket, const QString& filePath);
    void executeCommand_app_click(ITransportClient* socket, double mousex, double mousey);
    void executeCommand_app_click(ITransportClient* socket, qlonglong mousex, qlonglong mousey);
    void executeCommand_app_exit(ITransportClient* socket, double code);
    void executeCommand_app_crash(ITransportClient* socket);
    void executeCommand_app_pressAndHold(ITransportClient* socket, double mousex, double mousey);
    void executeCommand_app_pressAndHold(ITransportClient* socket, qlonglong mousex, qlonglong mousey);
    void executeCommand_app_move(ITransportClient* socket, double fromx, double fromy, double tox, double toy);
    void executeCommand_app_move(ITransportClient* socket, qlonglong fromx, qlonglong fromy, qlonglong tox, qlonglong toy);


    void executeCommand_app_listSignals(ITransportClient* socket,
                                            const QString& elementId);
};
