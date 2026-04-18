#ifndef TERMINALSECRETFLOW_H
#define TERMINALSECRETFLOW_H

#include <QColor>
#include <QString>
#include <functional>

class TerminalSecretFlow {
  public:
    enum class State {
        None,
        AwaitReveal,
        AwaitExitConfirm,
        RevealVisible,
    };

    using OutputSink = std::function<void(const QString&, const QColor&)>;

    ~TerminalSecretFlow();

    bool isActive() const;
    bool isRevealVisible() const;

    bool isSensitiveCommand(const QString& cmd) const;

    void stageSecret(const QString& label, const QString& secret, const OutputSink& output);
    bool handleInput(const QString& cmd, const OutputSink& output);
    bool handleCancelShortcut(const OutputSink& output);
    void clear();

  private:
    void revealPendingSecret(const OutputSink& output);

    State m_state = State::None;
    QString m_pendingSecret;
    QString m_pendingSecretLabel;
};

#endif // TERMINALSECRETFLOW_H
