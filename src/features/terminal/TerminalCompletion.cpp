#include "TerminalCompletion.h"

#include "TerminalCommandCatalog.h"

#include <QMap>

TerminalCompletionResult completeTerminalInput(const QString& currentInput) {
    const QStringList& rootCmds = terminalRootCommands();
    const QMap<QString, QStringList>& subCmds = terminalSubcommandMap();

    QStringList parts = currentInput.split(' ', Qt::SkipEmptyParts);
    bool endsWithSpace = currentInput.endsWith(' ');

    QStringList candidates;
    QString prefix;

    if (parts.isEmpty()) {
        candidates = rootCmds;
        prefix = "";
    } else if (parts.size() == 1 && !endsWithSpace) {
        prefix = parts[0].toLower();
        for (const QString& cmd : rootCmds) {
            if (cmd.startsWith(prefix)) {
                candidates << cmd;
            }
        }
    } else {
        const QString root = parts[0].toLower();
        if (!subCmds.contains(root)) {
            return {};
        }

        const QStringList& subs = subCmds[root];
        if (parts.size() == 1 && endsWithSpace) {
            candidates = subs;
            prefix = "";
        } else if (parts.size() == 2 && !endsWithSpace) {
            prefix = parts[1].toLower();
            for (const QString& sub : subs) {
                if (sub.startsWith(prefix)) {
                    candidates << sub;
                }
            }
        } else {
            return {};
        }
    }

    if (candidates.isEmpty()) {
        return {};
    }

    TerminalCompletionResult result;
    result.handled = true;

    if (candidates.size() == 1) {
        if (parts.size() <= 1 && !endsWithSpace) {
            result.updatedInput = candidates[0] + " ";
        } else {
            result.updatedInput = parts[0] + " " + candidates[0] + " ";
        }
        return result;
    }

    QString common = candidates[0];
    for (int i = 1; i < candidates.size(); ++i) {
        const int len = qMin(common.length(), candidates[i].length());
        int j = 0;
        while (j < len && common[j] == candidates[i][j]) {
            ++j;
        }
        common = common.left(j);
    }

    if (common.length() > prefix.length()) {
        if (parts.size() <= 1 && !endsWithSpace) {
            result.updatedInput = common;
        } else {
            result.updatedInput = parts[0] + " " + common;
        }
    } else {
        result.updatedInput = currentInput;
    }

    result.candidatesToShow = candidates;
    return result;
}
