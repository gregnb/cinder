#ifndef TERMINALASYNCOP_H
#define TERMINALASYNCOP_H

#include <QMetaObject>
#include <functional>

class TerminalRequestTracker;

class TerminalAsyncOp {
  public:
    TerminalAsyncOp(TerminalRequestTracker& tracker, const std::function<void()>& startTimeout);

    TerminalAsyncOp& watch(const QMetaObject::Connection& connection);
    void run(const std::function<void()>& request) const;

  private:
    TerminalRequestTracker& m_tracker;
    std::function<void()> m_startTimeout;
};

#endif // TERMINALASYNCOP_H
