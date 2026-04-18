#ifndef CODEXPROVIDER_H
#define CODEXPROVIDER_H

#include "agents/ModelProvider.h"

class CodexProvider : public ModelProvider {
    Q_OBJECT
  public:
    explicit CodexProvider(QObject* parent = nullptr);

    QString id() const override;
    QString displayName() const override;
    QString configSnippet() const override;
    QString configFormat() const override;

    bool supportsApiKey() const override;
    void setApiKey(const QString& key) override;
    bool hasApiKey() const override;
    QString maskedApiKey() const override;

  private:
    QString m_apiKey;
};

#endif // CODEXPROVIDER_H
