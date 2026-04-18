#include "CodexProvider.h"
#include <QCoreApplication>

CodexProvider::CodexProvider(QObject* parent) : ModelProvider(parent) {}

QString CodexProvider::id() const { return QStringLiteral("codex"); }

QString CodexProvider::displayName() const { return QStringLiteral("OpenAI Codex"); }

QString CodexProvider::configSnippet() const {
    QString mcpPath = QCoreApplication::applicationDirPath() + QStringLiteral("/cinder-mcp");
    if (m_policyApiKey.isEmpty()) {
        return QStringLiteral("[mcp_servers.cinder-wallet]\ncommand = \"%1\"").arg(mcpPath);
    }
    return QStringLiteral("[mcp_servers.cinder-wallet]\n"
                          "command = \"%1\"\n"
                          "\n"
                          "[mcp_servers.cinder-wallet.env]\n"
                          "CINDER_API_KEY = \"%2\"")
        .arg(mcpPath, m_policyApiKey);
}

QString CodexProvider::configFormat() const { return QStringLiteral("toml"); }

bool CodexProvider::supportsApiKey() const { return true; }

void CodexProvider::setApiKey(const QString& key) {
    m_apiKey = key;
    emit statusChanged(hasApiKey() ? QStringLiteral("API key set") : QStringLiteral("No API key"));
}

bool CodexProvider::hasApiKey() const { return !m_apiKey.isEmpty(); }

QString CodexProvider::maskedApiKey() const {
    if (m_apiKey.length() < 8) {
        return m_apiKey.isEmpty() ? QString() : QStringLiteral("****");
    }
    return QStringLiteral("****") + m_apiKey.right(4);
}
