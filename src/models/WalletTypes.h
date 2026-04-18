#ifndef MODEL_WALLETTYPES_H
#define MODEL_WALLETTYPES_H

#include <QString>

enum class WalletKeyType { Unknown = 0, Mnemonic, PrivateKey, Ledger, Trezor, Lattice };

enum class HardwarePluginId { None = 0, Ledger, Trezor, Lattice };

inline QString toStorageString(WalletKeyType type) {
    switch (type) {
        case WalletKeyType::Mnemonic:
            return QStringLiteral("mnemonic");
        case WalletKeyType::PrivateKey:
            return QStringLiteral("private_key");
        case WalletKeyType::Ledger:
            return QStringLiteral("ledger");
        case WalletKeyType::Trezor:
            return QStringLiteral("trezor");
        case WalletKeyType::Lattice:
            return QStringLiteral("lattice");
        case WalletKeyType::Unknown:
            return QString();
    }

    return QString();
}

inline QString toStorageString(HardwarePluginId plugin) {
    switch (plugin) {
        case HardwarePluginId::Ledger:
            return QStringLiteral("ledger");
        case HardwarePluginId::Trezor:
            return QStringLiteral("trezor");
        case HardwarePluginId::Lattice:
            return QStringLiteral("lattice");
        case HardwarePluginId::None:
            return QString();
    }

    return QString();
}

inline WalletKeyType parseWalletKeyType(const QString& value) {
    if (value == QLatin1String("mnemonic")) {
        return WalletKeyType::Mnemonic;
    }
    if (value == QLatin1String("private_key")) {
        return WalletKeyType::PrivateKey;
    }
    if (value == QLatin1String("ledger")) {
        return WalletKeyType::Ledger;
    }
    if (value == QLatin1String("trezor")) {
        return WalletKeyType::Trezor;
    }
    if (value == QLatin1String("lattice")) {
        return WalletKeyType::Lattice;
    }
    return WalletKeyType::Unknown;
}

inline HardwarePluginId parseHardwarePluginId(const QString& value) {
    if (value == QLatin1String("ledger")) {
        return HardwarePluginId::Ledger;
    }
    if (value == QLatin1String("trezor")) {
        return HardwarePluginId::Trezor;
    }
    if (value == QLatin1String("lattice")) {
        return HardwarePluginId::Lattice;
    }
    return HardwarePluginId::None;
}

inline bool isHardwareWalletType(WalletKeyType type) {
    return type == WalletKeyType::Ledger || type == WalletKeyType::Trezor ||
           type == WalletKeyType::Lattice;
}

inline HardwarePluginId pluginForWalletType(WalletKeyType type) {
    switch (type) {
        case WalletKeyType::Ledger:
            return HardwarePluginId::Ledger;
        case WalletKeyType::Trezor:
            return HardwarePluginId::Trezor;
        case WalletKeyType::Lattice:
            return HardwarePluginId::Lattice;
        case WalletKeyType::Mnemonic:
        case WalletKeyType::PrivateKey:
        case WalletKeyType::Unknown:
            return HardwarePluginId::None;
    }

    return HardwarePluginId::None;
}

inline QString hardwarePluginDisplayName(HardwarePluginId plugin) {
    switch (plugin) {
        case HardwarePluginId::Ledger:
            return QStringLiteral("Ledger");
        case HardwarePluginId::Trezor:
            return QStringLiteral("Trezor");
        case HardwarePluginId::Lattice:
            return QStringLiteral("Lattice");
        case HardwarePluginId::None:
            return QString();
    }

    return QString();
}

#endif // MODEL_WALLETTYPES_H
