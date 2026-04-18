#ifndef TERMINALCOMPLETION_H
#define TERMINALCOMPLETION_H

#include <QString>
#include <QStringList>

struct TerminalCompletionResult {
    bool handled = false;
    QString updatedInput;
    QStringList candidatesToShow;
};

TerminalCompletionResult completeTerminalInput(const QString& currentInput);

#endif // TERMINALCOMPLETION_H
