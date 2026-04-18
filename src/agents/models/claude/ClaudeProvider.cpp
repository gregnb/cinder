#include "ClaudeProvider.h"
#include <QCoreApplication>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>

#ifdef Q_OS_MAC
#include "util/MacUtils.h"
#endif

ClaudeProvider::ClaudeProvider(QObject* parent) : ModelProvider(parent) {}

QString ClaudeProvider::id() const { return QStringLiteral("claude"); }

QString ClaudeProvider::displayName() const { return QStringLiteral("Claude Desktop"); }

QString ClaudeProvider::configSnippet() const {
    QString mcpPath = QCoreApplication::applicationDirPath() + QStringLiteral("/cinder-mcp");
    if (m_policyApiKey.isEmpty()) {
        return QStringLiteral("{\n"
                              "  \"mcpServers\": {\n"
                              "    \"cinder-wallet\": {\n"
                              "      \"command\": \"%1\"\n"
                              "    }\n"
                              "  }\n"
                              "}")
            .arg(mcpPath);
    }
    return QStringLiteral("{\n"
                          "  \"mcpServers\": {\n"
                          "    \"cinder-wallet\": {\n"
                          "      \"command\": \"%1\",\n"
                          "      \"env\": {\n"
                          "        \"CINDER_API_KEY\": \"%2\"\n"
                          "      }\n"
                          "    }\n"
                          "  }\n"
                          "}")
        .arg(mcpPath, m_policyApiKey);
}

bool ClaudeProvider::supportsApiKey() const { return true; }

void ClaudeProvider::setApiKey(const QString& key) {
    m_apiKey = key;
    emit statusChanged(hasApiKey() ? QStringLiteral("API key set") : QStringLiteral("No API key"));
}

bool ClaudeProvider::hasApiKey() const { return !m_apiKey.isEmpty(); }

QString ClaudeProvider::maskedApiKey() const {
    if (m_apiKey.length() < 8) {
        return m_apiKey.isEmpty() ? QString() : QStringLiteral("****");
    }
    return QStringLiteral("****") + m_apiKey.right(4);
}

// ── OAuth token ──────────────────────────────────────────────────

void ClaudeProvider::setOAuthToken(const QString& rawToken) {
    qDebug() << "[ClaudeProvider] setOAuthToken called, raw length:" << rawToken.length()
             << "first 30 chars:" << rawToken.left(30);

    // Claude Code stores: {"claudeAiOauth":{"accessToken":"...", ...}}
    // Also handle flat: {"accessToken":"..."} or bare token string
    QJsonDocument doc = QJsonDocument::fromJson(rawToken.toUtf8());
    if (doc.isObject()) {
        QJsonObject obj = doc.object();
        qDebug() << "[ClaudeProvider] Parsed as JSON, keys:" << obj.keys();

        // Try nested: claudeAiOauth.accessToken
        if (obj.contains(QStringLiteral("claudeAiOauth"))) {
            QJsonObject inner = obj[QStringLiteral("claudeAiOauth")].toObject();
            qDebug() << "[ClaudeProvider] Found claudeAiOauth, inner keys:" << inner.keys();
            m_oauthToken = inner[QStringLiteral("accessToken")].toString();
            if (m_oauthToken.isEmpty()) {
                m_oauthToken = inner[QStringLiteral("access_token")].toString();
            }
        }
        // Try flat: accessToken at top level
        if (m_oauthToken.isEmpty()) {
            m_oauthToken = obj[QStringLiteral("accessToken")].toString();
            if (m_oauthToken.isEmpty()) {
                m_oauthToken = obj[QStringLiteral("access_token")].toString();
            }
        }
        qDebug() << "[ClaudeProvider] Extracted token length:" << m_oauthToken.length();
    } else {
        m_oauthToken = rawToken.trimmed();
        qDebug() << "[ClaudeProvider] Stored as bare token, length:" << m_oauthToken.length();
    }

    qDebug() << "[ClaudeProvider] hasOAuthToken:" << hasOAuthToken()
             << "hasCredential:" << hasCredential();

    emit oauthTokenChanged(!m_oauthToken.isEmpty());
    emit statusChanged(hasCredential() ? QStringLiteral("Connected via OAuth")
                                       : QStringLiteral("No credentials"));
}

bool ClaudeProvider::hasOAuthToken() const { return !m_oauthToken.isEmpty(); }

QString ClaudeProvider::maskedOAuthToken() const {
    if (m_oauthToken.length() < 8) {
        return m_oauthToken.isEmpty() ? QString() : QStringLiteral("****");
    }
    return QStringLiteral("oauth:****") + m_oauthToken.right(4);
}

void ClaudeProvider::clearOAuthToken() {
    m_oauthToken.clear();
    emit oauthTokenChanged(false);
    emit statusChanged(hasApiKey() ? QStringLiteral("API key set")
                                   : QStringLiteral("No credentials"));
}

bool ClaudeProvider::hasCredential() const { return hasOAuthToken() || hasApiKey(); }

QString ClaudeProvider::effectiveToken() const {
    if (!m_oauthToken.isEmpty()) {
        return m_oauthToken;
    }
    return m_apiKey;
}

bool ClaudeProvider::loadFromKeychain() {
    qDebug() << "[ClaudeProvider] loadFromKeychain called";
#ifdef Q_OS_MAC
    qDebug() << "[ClaudeProvider] Q_OS_MAC is defined, calling readClaudeCodeOAuthToken...";
    QString rawToken;
    bool ok = readClaudeCodeOAuthToken(rawToken);
    qDebug() << "[ClaudeProvider] readClaudeCodeOAuthToken returned:" << ok
             << "rawToken empty:" << rawToken.isEmpty() << "rawToken length:" << rawToken.length();
    if (ok && !rawToken.isEmpty()) {
        setOAuthToken(rawToken);
        qDebug() << "[ClaudeProvider] loadFromKeychain SUCCESS";
        return true;
    }
    qDebug() << "[ClaudeProvider] loadFromKeychain FAILED";
#else
    qDebug() << "[ClaudeProvider] Q_OS_MAC not defined, skipping Keychain";
#endif
    return false;
}
