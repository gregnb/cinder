#ifndef CLAUDEPROVIDER_H
#define CLAUDEPROVIDER_H

#include "agents/ModelProvider.h"

class ClaudeProvider : public ModelProvider {
    Q_OBJECT
  public:
    explicit ClaudeProvider(QObject* parent = nullptr);

    QString id() const override;
    QString displayName() const override;
    QString configSnippet() const override;

    bool supportsApiKey() const override;
    void setApiKey(const QString& key) override;
    bool hasApiKey() const override;
    QString maskedApiKey() const override;

    // OAuth token support
    void setOAuthToken(const QString& rawToken);
    bool hasOAuthToken() const;
    QString maskedOAuthToken() const;
    void clearOAuthToken();

    bool hasCredential() const;
    QString effectiveToken() const;
    bool loadFromKeychain();

  signals:
    void oauthTokenChanged(bool hasToken);

  private:
    QString m_apiKey;
    QString m_oauthToken;
};

#endif // CLAUDEPROVIDER_H
