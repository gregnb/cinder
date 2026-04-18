#include "TerminalRequestTracker.h"

TerminalRequestTracker::TerminalRequestTracker(QObject* parent) : QObject(parent) {
    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, [this]() {
        if (m_connections.isEmpty() || !m_timeoutHandler) {
            return;
        }
        m_timeoutHandler();
    });
}

TerminalRequestTracker&
TerminalRequestTracker::operator<<(const QMetaObject::Connection& connection) {
    m_connections << connection;
    return *this;
}

bool TerminalRequestTracker::isEmpty() const { return m_connections.isEmpty(); }

void TerminalRequestTracker::clear() {
    for (auto& connection : m_connections) {
        disconnect(connection);
    }
    m_connections.clear();
}

void TerminalRequestTracker::startTimeout(int milliseconds) { m_timeoutTimer->start(milliseconds); }

void TerminalRequestTracker::stopTimeout() { m_timeoutTimer->stop(); }

void TerminalRequestTracker::setTimeoutHandler(const std::function<void()>& handler) {
    m_timeoutHandler = handler;
}
