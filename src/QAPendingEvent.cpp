#include "include/qt_qa_engine/QAPendingEvent.h"

QAPendingEvent::QAPendingEvent(QObject* parent)
    : QObject(parent)
{
}

void QAPendingEvent::setCompleted()
{
    emit completed(this);
}
