#include "features/settings/SettingsHandler.h"

#include "db/WalletDb.h"
#include "services/SolanaApi.h"
#ifdef Q_OS_MAC
#include "util/MacUtils.h"
#endif

#include <QSettings>

namespace {
    struct LanguageEntry {
        const char* code;
        const char* displayName;
    };

    static const LanguageEntry LANGUAGE_ENTRIES[] = {
        {"en", "English"},
        {"es", "Español"},
        {"zh_CN", "\xe4\xb8\xad\xe6\x96\x87 (\xe7\xae\x80\xe4\xbd\x93)"}, // 中文 (简体)
        {"ja", "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e"},                   // 日本語
        {"ko", "\xed\x95\x9c\xea\xb5\xad\xec\x96\xb4"},                   // 한국어
        {"pt_BR", "Portugu\xc3\xaas (BR)"},                               // Português (BR)
        {"fr", "Fran\xc3\xa7"
               "ais"}, // Français
        {"de", "Deutsch"},
    };

    constexpr int LANGUAGE_COUNT = sizeof(LANGUAGE_ENTRIES) / sizeof(LANGUAGE_ENTRIES[0]);
} // namespace

const QString SettingsHandler::DEFAULT_RPC_ENDPOINT =
    QStringLiteral("https://api.mainnet-beta.solana.com");

SettingsHandler::SettingsHandler(SolanaApi* api, QObject* parent) : QObject(parent), m_api(api) {}

QString SettingsHandler::localeCodeForName(const QString& displayName) {
    for (int i = 0; i < LANGUAGE_COUNT; ++i) {
        if (displayName == QString::fromUtf8(LANGUAGE_ENTRIES[i].displayName)) {
            return QString::fromLatin1(LANGUAGE_ENTRIES[i].code);
        }
    }

    return QStringLiteral("en");
}

QString SettingsHandler::displayNameForCode(const QString& code) {
    for (int i = 0; i < LANGUAGE_COUNT; ++i) {
        if (code == QLatin1String(LANGUAGE_ENTRIES[i].code)) {
            return QString::fromUtf8(LANGUAGE_ENTRIES[i].displayName);
        }
    }

    return QStringLiteral("English");
}

QString SettingsHandler::savedLanguageCode() {
    QSettings settings;
    return settings.value(QStringLiteral("language"), QStringLiteral("en")).toString();
}

QStringList SettingsHandler::languageDisplayNames() {
    QStringList names;
    names.reserve(LANGUAGE_COUNT);
    for (int i = 0; i < LANGUAGE_COUNT; ++i) {
        names.append(QString::fromUtf8(LANGUAGE_ENTRIES[i].displayName));
    }
    return names;
}

QStringList SettingsHandler::currentRpcUrls() const {
    if (!m_api) {
        return {DEFAULT_RPC_ENDPOINT};
    }
    return m_api->rpcUrls();
}

void SettingsHandler::setRpcUrls(const QStringList& urls) {
    if (!m_api) {
        return;
    }
    m_api->setRpcUrls(urls);
    saveRpcEndpoints(m_api->rpcUrls());
}

void SettingsHandler::addRpcUrl(const QString& url) {
    if (!m_api) {
        return;
    }
    const QString trimmed = url.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    m_api->addRpcUrl(trimmed);
    saveRpcEndpoints(m_api->rpcUrls());
}

void SettingsHandler::removeRpcUrl(const QString& url) {
    if (!m_api) {
        return;
    }
    m_api->removeRpcUrl(url);
    saveRpcEndpoints(m_api->rpcUrls());
}

QStringList SettingsHandler::loadRpcEndpoints() {
    QSettings settings;
    QStringList endpoints = settings.value(QStringLiteral("rpcEndpoints")).toStringList();
    if (endpoints.isEmpty()) {
        endpoints.append(DEFAULT_RPC_ENDPOINT);
    }
    return endpoints;
}

void SettingsHandler::saveRpcEndpoints(const QStringList& endpoints) {
    QSettings settings;
    settings.setValue(QStringLiteral("rpcEndpoints"), endpoints);
    settings.sync();
}

QString SettingsHandler::applyLanguageDisplayName(const QString& displayName) {
    const QString newCode = localeCodeForName(displayName);

    QSettings settings;
    const QString oldCode =
        settings.value(QStringLiteral("language"), QStringLiteral("en")).toString();
    if (newCode == oldCode) {
        return QString();
    }

    settings.setValue(QStringLiteral("language"), newCode);
    settings.sync();
    return newCode;
}

bool SettingsHandler::isBiometricAvailableOnDevice() const {
#ifdef Q_OS_MAC
    return isBiometricAvailable();
#else
    return false;
#endif
}

QString SettingsHandler::effectiveWalletAddress(const QString& preferredAddress) const {
    if (!preferredAddress.isEmpty()) {
        return preferredAddress;
    }

    const auto wallets = WalletDb::getAllRecords();
    if (!wallets.isEmpty()) {
        return wallets.first().address;
    }

    return QString();
}

bool SettingsHandler::biometricEnabledForWallet(const QString& walletAddress) const {
    const QString effectiveAddress = effectiveWalletAddress(walletAddress);
    if (effectiveAddress.isEmpty()) {
        return false;
    }

    return WalletDb::isBiometricEnabled(effectiveAddress);
}

void SettingsHandler::disableBiometric(const QString& walletAddress) const {
    const QString effectiveAddress = effectiveWalletAddress(walletAddress);
    if (effectiveAddress.isEmpty()) {
        return;
    }

#ifdef Q_OS_MAC
    deleteBiometricPassword(effectiveAddress);
#endif
    WalletDb::setBiometricEnabled(effectiveAddress, false);
}
