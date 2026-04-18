#include "ClaudeCodeCliProvider.h"
#include <QCoreApplication>

ClaudeCodeCliProvider::ClaudeCodeCliProvider(QObject* parent) : ModelProvider(parent) {}

QString ClaudeCodeCliProvider::id() const { return QStringLiteral("claude-code-cli"); }

QString ClaudeCodeCliProvider::displayName() const { return QStringLiteral("Claude Code (CLI)"); }

QString ClaudeCodeCliProvider::configFormat() const { return QStringLiteral("bash"); }

QString ClaudeCodeCliProvider::configSnippet() const {
    QString mcpPath = QCoreApplication::applicationDirPath() + QStringLiteral("/cinder-mcp");
    if (m_policyApiKey.isEmpty()) {
        return QStringLiteral("claude mcp add cinder-wallet -- %1").arg(mcpPath);
    }
    return QStringLiteral("claude mcp add cinder-wallet \\\n"
                          "  -e CINDER_API_KEY=%1 \\\n"
                          "  -- %2")
        .arg(m_policyApiKey, mcpPath);
}
