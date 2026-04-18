#ifndef TERMINALREQUESTTRACKER_H
#define TERMINALREQUESTTRACKER_H

#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QTimer>
#include <functional>

class TerminalRequestTracker final : public QObject {
    Q_OBJECT

  public:
    explicit TerminalRequestTracker(QObject* parent = nullptr);

    TerminalRequestTracker& operator<<(const QMetaObject::Connection& connection);

    bool isEmpty() const;
    void clear();

    void startTimeout(int milliseconds);
    void stopTimeout();

    void setTimeoutHandler(const std::function<void()>& handler);

  private:
    QList<QMetaObject::Connection> m_connections;
    QTimer* m_timeoutTimer = nullptr;
    std::function<void()> m_timeoutHandler;
};

#endif // TERMINALREQUESTTRACKER_H
