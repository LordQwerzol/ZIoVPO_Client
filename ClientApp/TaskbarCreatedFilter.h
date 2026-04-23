#ifndef TASKBARCREATEDFILTER_H
#define TASKBARCREATEDFILTER_H

#include <QAbstractNativeEventFilter>
#include <windows.h>
#include <QtGlobal>
#include <QByteArray>

class TaskbarCreatedFilter : public QAbstractNativeEventFilter {
public:
    using Callback = std::function<void()>;

    TaskbarCreatedFilter(Callback callback) : m_callback(callback) {
        m_taskbarCreatedMsg = RegisterWindowMessageW(L"TaskbarCreated");
    }

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
    {
        if (eventType == "windows_dispatcher_MSG" || eventType == "windows_generic_MSG") {
            MSG* msg = static_cast<MSG*>(message);
            if (msg->message == m_taskbarCreatedMsg && m_callback) {
                m_callback();
            }
        }
        return false;
    }

private:
    UINT m_taskbarCreatedMsg;
    Callback m_callback;
};

#endif // TASKBARCREATEDFILTER_H
