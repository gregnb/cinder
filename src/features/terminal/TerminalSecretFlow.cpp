#include "TerminalSecretFlow.h"

#include "TerminalHandlerCommon.h"

using namespace terminal;

TerminalSecretFlow::~TerminalSecretFlow() { clear(); }

bool TerminalSecretFlow::isActive() const { return m_state != State::None; }

bool TerminalSecretFlow::isRevealVisible() const { return m_state == State::RevealVisible; }

bool TerminalSecretFlow::isSensitiveCommand(const QString& cmd) const {
    QStringList parts = cmd.split(' ', Qt::SkipEmptyParts);
    if (parts.size() < 2) {
        return false;
    }

    const QString root = parts[0].toLower();
    const QString sub = parts[1].toLower();
    return root == "key" && (sub == "from-secret" || sub == "from-mnemonic");
}

void TerminalSecretFlow::stageSecret(const QString& label, const QString& secret,
                                     const OutputSink& output) {
    clear();
    m_pendingSecret = secret;
    m_pendingSecretLabel = label;
    m_state = State::AwaitReveal;

    output("  " + label + ":  ********", kWarnColor);
    output("  Reveal? [yes/no]", kDimColor);
}

void TerminalSecretFlow::revealPendingSecret(const OutputSink& output) {
    if (m_pendingSecret.isEmpty()) {
        output("No pending secret to reveal.", kErrorColor);
        clear();
        return;
    }

    m_state = State::RevealVisible;
    output("  " + m_pendingSecretLabel + ":  " + m_pendingSecret, kWarnColor);
    output("  Press Esc, Ctrl+C, or Cmd+C to exit reveal mode.", kDimColor);
}

bool TerminalSecretFlow::handleInput(const QString& cmd, const OutputSink& output) {
    if (!isActive()) {
        return false;
    }

    const QString answer = cmd.trimmed().toLower();

    if (m_state == State::AwaitReveal) {
        if (answer == "yes" || answer == "y") {
            revealPendingSecret(output);
        } else if (answer == "no" || answer == "n") {
            m_state = State::AwaitExitConfirm;
            output("You are about to exit without copying the secret.", kWarnColor);
            output("Did you copy it? [yes/no]", kDimColor);
        } else {
            output("Please answer 'yes' or 'no'.", kDimColor);
        }
        return true;
    }

    if (m_state == State::AwaitExitConfirm) {
        if (answer == "yes" || answer == "y") {
            output("Secret discarded.", kDimColor);
            clear();
        } else if (answer == "no" || answer == "n") {
            m_state = State::AwaitReveal;
            output("Reveal? [yes/no]", kDimColor);
        } else {
            output("Please answer 'yes' or 'no'.", kDimColor);
        }
        return true;
    }

    if (m_state == State::RevealVisible) {
        if (answer == "exit" || answer == "hide" || answer == "done") {
            output("Exited reveal mode.", kDimColor);
            clear();
        } else {
            output("Reveal mode active. Press Esc/Ctrl+C/Cmd+C to exit.", kDimColor);
        }
        return true;
    }

    return false;
}

bool TerminalSecretFlow::handleCancelShortcut(const OutputSink& output) {
    if (m_state != State::RevealVisible) {
        return false;
    }
    output("^C", kDimColor);
    output("Exited reveal mode.", kDimColor);
    clear();
    return true;
}

void TerminalSecretFlow::clear() {
    wipeQString(m_pendingSecret);
    wipeQString(m_pendingSecretLabel);
    m_state = State::None;
}
