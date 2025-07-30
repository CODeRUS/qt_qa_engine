#include <qt_qa_engine/IEnginePlatform.h>

IEnginePlatform::IEnginePlatform(QWindow* window)
    : QObject(window)
{
}
