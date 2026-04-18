#ifndef SETTINGSHANDLER_H
#define SETTINGSHANDLER_H

#include <QObject>
#include <QString>
#include <QStringList>

class SolanaApi;

class SettingsHandler : public QObject {
    Q_OBJECT

  public:
    explicit SettingsHandler(SolanaApi* api, QObject* parent = nullptr);

    static QString localeCodeForName(const QString& displayName);
    static QString displayNameForCode(const QString& code);
    static QString savedLanguageCode();
    static QStringList languageDisplayNames();

    // RPC endpoint pool
    QStringList currentRpcUrls() const;
    void setRpcUrls(const QStringList& urls);
    void addRpcUrl(const QString& url);
    void removeRpcUrl(const QString& url);
    static QStringList loadRpcEndpoints();
    static void saveRpcEndpoints(const QStringList& endpoints);
    static const QString DEFAULT_RPC_ENDPOINT;

    QString applyLanguageDisplayName(const QString& displayName);

    bool isBiometricAvailableOnDevice() const;
    bool biometricEnabledForWallet(const QString& walletAddress) const;
    void disableBiometric(const QString& walletAddress) const;
    QString effectiveWalletAddress(const QString& preferredAddress) const;

  private:
    SolanaApi* m_api = nullptr;
};

#endif // SETTINGSHANDLER_H
