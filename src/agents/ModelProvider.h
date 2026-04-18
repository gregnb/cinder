#ifndef MODELPROVIDER_H
#define MODELPROVIDER_H

#include <QObject>
#include <QString>

class ModelProvider : public QObject {
    Q_OBJECT
  public:
    explicit ModelProvider(QObject* parent = nullptr);
    ~ModelProvider() override;

    virtual QString id() const = 0;
    virtual QString displayName() const = 0;
    virtual QString configSnippet() const = 0;
    virtual QString configFormat() const { return QStringLiteral("json"); }

    virtual void setWalletAddress(const QString& address) { m_walletAddress = address; }
    QString walletAddress() const { return m_walletAddress; }

    virtual bool supportsApiKey() const { return false; }
    virtual void setApiKey(const QString& key) { Q_UNUSED(key); }
    virtual bool hasApiKey() const { return false; }
    virtual QString maskedApiKey() const { return {}; }

    // Policy API key for MCP config snippets
    virtual void setPolicyApiKey(const QString& key) { m_policyApiKey = key; }
    QString policyApiKey() const { return m_policyApiKey; }

  signals:
    void statusChanged(const QString& status);

  protected:
    QString m_walletAddress;
    QString m_policyApiKey;
};

#endif // MODELPROVIDER_H
