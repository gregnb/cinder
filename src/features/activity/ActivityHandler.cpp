#include "ActivityHandler.h"
#include "db/TokenAccountDb.h"
#include "tx/KnownTokens.h"
#include <QCoreApplication>
#include <QHash>
#include <QLocale>

namespace {
    enum class ActivityKind {
        Send,
        Receive,
        Mint,
        Burn,
        CloseAccount,
        CreateAccount,
        CreateNonce,
        InitAccount,
        Unknown
    };

    ActivityKind activityKindFromType(const QString& type) {
        static const QHash<QString, ActivityKind> kByType = {
            {"send", ActivityKind::Send},
            {"receive", ActivityKind::Receive},
            {"mint", ActivityKind::Mint},
            {"burn", ActivityKind::Burn},
            {"close_account", ActivityKind::CloseAccount},
            {"create_account", ActivityKind::CreateAccount},
            {"create_nonce", ActivityKind::CreateNonce},
            {"init_account", ActivityKind::InitAccount},
        };
        return kByType.value(type, ActivityKind::Unknown);
    }

    QStringList orderedActionTypes() {
        static const QStringList kTypes = {
            "send",           "receive",       "mint",         "burn",
            "create_account", "close_account", "create_nonce", "init_account",
        };
        return kTypes;
    }

    QString amountDisplay(double amount) {
        if (amount >= 1000.0) {
            return QLocale(QLocale::English).toString(amount, 'f', 2);
        }
        if (amount >= 1.0) {
            return QString::number(amount, 'f', 4);
        }
        if (amount > 0) {
            QString s = QString::number(amount, 'f', 6);
            while (s.endsWith('0') && !s.endsWith(".00")) {
                s.chop(1);
            }
            return s;
        }
        return "0";
    }
} // namespace

QStringList ActivityHandler::allActionTypes() const { return orderedActionTypes(); }

QString ActivityHandler::badgeText(const QString& type) const {
    switch (activityKindFromType(type)) {
    case ActivityKind::Send:
        return QCoreApplication::translate("ActivityHandler", "SEND");
    case ActivityKind::Receive:
        return QCoreApplication::translate("ActivityHandler", "RECEIVE");
    case ActivityKind::Mint:
        return QCoreApplication::translate("ActivityHandler", "MINT");
    case ActivityKind::Burn:
        return QCoreApplication::translate("ActivityHandler", "BURN");
    case ActivityKind::CloseAccount:
        return QCoreApplication::translate("ActivityHandler", "CLOSE");
    case ActivityKind::CreateAccount:
        return QCoreApplication::translate("ActivityHandler", "CREATE");
    case ActivityKind::CreateNonce:
        return QCoreApplication::translate("ActivityHandler", "NONCE");
    case ActivityKind::InitAccount:
        return QCoreApplication::translate("ActivityHandler", "INIT");
    case ActivityKind::Unknown:
        return type.toUpper();
    }
}

QString ActivityHandler::formatNumber(double amount) const { return amountDisplay(amount); }

QString ActivityHandler::truncateAddr(const QString& addr) const {
    if (addr.length() <= 12) {
        return addr;
    }
    return addr.left(4) + "..." + addr.right(4);
}

TransactionFilter ActivityHandler::buildFilter(const ActivityFilters& filters) const {
    TransactionFilter filter;
    filter.signature = filters.signature;
    filter.timeFrom = filters.timeFrom;
    filter.timeTo = filters.timeTo;
    filter.actionTypes = filters.actionTypes.values();
    filter.fromAddress = filters.fromAddress;
    filter.toAddress = filters.toAddress;
    filter.amountMin = filters.amountMin;
    filter.amountMax = filters.amountMax;
    filter.token = filters.token;
    return filter;
}

bool ActivityHandler::hasActiveFilter(const ActivityFilters& filters) const {
    return !filters.signature.isEmpty() || filters.timeFrom > 0 || filters.timeTo > 0 ||
           !filters.actionTypes.isEmpty() || !filters.fromAddress.isEmpty() ||
           !filters.toAddress.isEmpty() || filters.amountMin >= 0 || filters.amountMax >= 0 ||
           !filters.token.isEmpty();
}

int ActivityHandler::totalRows(const QString& ownerAddress) const {
    return TransactionDb::countTransactions(ownerAddress);
}

int ActivityHandler::filteredRows(const QString& ownerAddress,
                                  const ActivityFilters& filters) const {
    return TransactionDb::countFilteredTransactions(ownerAddress, buildFilter(filters));
}

QList<TransactionRecord> ActivityHandler::loadPage(const QString& ownerAddress,
                                                   const ActivityFilters& filters, int pageSize,
                                                   int currentPage) const {
    const int offset = currentPage * pageSize;
    return TransactionDb::getFilteredTransactionsRecords(ownerAddress, buildFilter(filters),
                                                         pageSize, offset);
}

QList<ActivityRowView> ActivityHandler::buildRows(const QList<TransactionRecord>& txns) const {
    QList<ActivityRowView> rows;
    rows.reserve(txns.size());

    for (const auto& tx : txns) {
        ActivityRowView row;
        row.signature = tx.signature;
        row.blockTime = tx.blockTime;
        row.activityType = tx.activityType;
        row.fromAddress = tx.fromAddress;
        row.toAddress = tx.toAddress;
        row.amount = tx.amount;
        row.err = tx.err;

        if (tx.token == "SOL") {
            row.tokenSymbol = "SOL";
            row.iconPath = ":/icons/tokens/sol.png";
        } else if (!tx.token.isEmpty()) {
            KnownToken known = resolveKnownToken(tx.token);
            if (!known.symbol.isEmpty()) {
                row.tokenSymbol = known.symbol;
                row.iconPath = known.iconPath;
            } else {
                auto tokenInfo = TokenAccountDb::getTokenRecord(tx.token);
                if (tokenInfo.has_value()) {
                    row.tokenSymbol = tokenInfo->symbol;
                    row.logoUrl = tokenInfo->logoUrl;
                }
                if (row.tokenSymbol.isEmpty()) {
                    row.tokenSymbol = tx.token.left(4) + "..." + tx.token.right(4);
                }
            }
        }

        row.signatureDisplay = tx.signature.left(8) + "...";
        row.badgeText = badgeText(tx.activityType);
        row.fromDisplay = tx.fromAddress.isEmpty() ? "-" : truncateAddr(tx.fromAddress);
        row.toDisplay = tx.toAddress.isEmpty() ? "-" : truncateAddr(tx.toAddress);

        switch (activityKindFromType(tx.activityType)) {
        case ActivityKind::Receive:
        case ActivityKind::Mint:
            row.amountText = "+" + formatNumber(tx.amount);
            row.amountColor = "#10b981";
            break;
        case ActivityKind::Send:
        case ActivityKind::Burn:
            row.amountText = "-" + formatNumber(tx.amount);
            row.amountColor = "#ef4444";
            break;
        case ActivityKind::CreateAccount:
            if (tx.amount > 0) {
                row.amountText = "-" + formatNumber(tx.amount);
                row.amountColor = "#ef4444";
            } else {
                row.amountText = tx.amount > 0 ? formatNumber(tx.amount) : "-";
                row.amountColor = "rgba(255,255,255,0.5)";
            }
            break;
        case ActivityKind::CloseAccount:
        case ActivityKind::CreateNonce:
        case ActivityKind::InitAccount:
        case ActivityKind::Unknown:
            row.amountText = tx.amount > 0 ? formatNumber(tx.amount) : "-";
            row.amountColor = "rgba(255,255,255,0.5)";
            break;
        }

        rows.append(row);
    }

    return rows;
}
