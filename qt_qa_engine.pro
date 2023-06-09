##
 # Copyright (c) New Cloud Technologies, Ltd., 2014-2022
 #
 # You can not use the contents of the file in any way without
 # New Cloud Technologies, Ltd. written permission.
 #
 # To obtain such a permit, you should contact New Cloud Technologies, Ltd.
 # at http://ncloudtech.com/contact.html
 #
##

TARGET = qaengine
TEMPLATE = lib

TARGETPATH = AutoQA/QAEngine
android {
    TARGET = $$qtLibraryTarget($$TARGET)
    installPath = $$[QT_INSTALL_QML]/$$TARGETPATH
    target.path = $$installPath
    INSTALLS += target
}

QT += core core-private network
CONFIG += plugin
#CONFIG += c++11

equals(QT_MAJOR_VERSION, 6) {
    QT += core5compat
}

qtHaveModule(xmlpatterns) {
    QT += xmlpatterns
    DEFINES += MO_USE_QXMLPATTERNS
}

qtHaveModule(qml) {
    QT += qml quick quick-private
    DEFINES += MO_USE_QUICK

    SOURCES += src/QuickEnginePlatform.cpp
    HEADERS += include/qt_qa_engine/QuickEnginePlatform.h
}

qtHaveModule(widgets) {
    QT += widgets widgets-private
    DEFINES += MO_USE_QWIDGETS

    SOURCES += src/WidgetsEnginePlatform.cpp
    HEADERS += include/qt_qa_engine/WidgetsEnginePlatform.h
}

SOURCES += \
    src/GenericEnginePlatform.cpp \
    src/IEnginePlatform.cpp \
    src/ITransportClient.cpp \
    src/ITransportServer.cpp \
    src/QAEngine.cpp \
    src/QAEngineSocketClient.cpp \
    src/QAKeyMouseEngine.cpp \
    src/QAPendingEvent.cpp \
    src/TCPSocketClient.cpp \
    src/TCPSocketServer.cpp \
    src/loader.cpp

HEADERS += \
    include/qt_qa_engine/GenericEnginePlatform.h \
    include/qt_qa_engine/IEnginePlatform.h \
    include/qt_qa_engine/ITransportClient.h \
    include/qt_qa_engine/ITransportServer.h \
    include/qt_qa_engine/QAEngine.h \
    include/qt_qa_engine/QAEngineSocketClient.h \
    include/qt_qa_engine/QAKeyMouseEngine.h \
    include/qt_qa_engine/QAPendingEvent.h \
    include/qt_qa_engine/TCPSocketClient.h \
    include/qt_qa_engine/TCPSocketServer.h

INCLUDEPATH += include

RESOURCES += \
    qml/qtqaengine.qrc

CONFIG += qtquickcompiler
