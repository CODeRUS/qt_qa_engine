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

# TODO: uncomment xmlpatterns after KDM-1227 resolved
QT += qml core network quick quick-private core-private #xmlpatterns
CONFIG += plugin
CONFIG += c++11

contains(DEFINES, MO_USE_QWIDGETS) {
    QT += widgets widgets-private

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
    src/QAKeyEngine.cpp \
    src/QAMouseEngine.cpp \
    src/QAPendingEvent.cpp \
    src/QuickEnginePlatform.cpp \
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
    include/qt_qa_engine/QAKeyEngine.h \
    include/qt_qa_engine/QAMouseEngine.h \
    include/qt_qa_engine/QAPendingEvent.h \
    include/qt_qa_engine/QuickEnginePlatform.h \
    include/qt_qa_engine/TCPSocketClient.h \
    include/qt_qa_engine/TCPSocketServer.h

INCLUDEPATH += include

RESOURCES += \
    qml/qtqaengine.qrc

CONFIG += qtquickcompiler
