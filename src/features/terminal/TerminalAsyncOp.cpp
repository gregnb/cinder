#include "TerminalAsyncOp.h"

#include "TerminalRequestTracker.h"

TerminalAsyncOp::TerminalAsyncOp(TerminalRequestTracker& tracker,
                                 const std::function<void()>& startTimeout)
    : m_tracker(tracker), m_startTimeout(startTimeout) {}

TerminalAsyncOp& TerminalAsyncOp::watch(const QMetaObject::Connection& connection) {
    m_tracker << connection;
    return *this;
}

void TerminalAsyncOp::run(const std::function<void()>& request) const {
    m_startTimeout();
    request();
}
