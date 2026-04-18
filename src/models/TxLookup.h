#ifndef TXLOOKUP_H
#define TXLOOKUP_H

#include <QList>
#include <QString>

struct SolBalanceChange {
    QString address;
    qint64 preLamports = 0;
    qint64 postLamports = 0;
    qint64 deltaLamports = 0;
};

struct TokenBalanceChange {
    QString owner;
    QString mint;
    double preAmount = 0.0;
    double postAmount = 0.0;
    double deltaAmount = 0.0;
};

struct TxBalanceChanges {
    QList<SolBalanceChange> solChanges;
    QList<TokenBalanceChange> tokenChanges;
};

struct TxBalanceViewRow {
    QString address;
    QString beforeText;
    QString afterText;
    QString changeText;
    bool isPositiveChange = false;
    QString mint;
    bool isNativeSol = false;
};

struct TxBalanceViewData {
    QList<TxBalanceViewRow> solRows;
    QList<TxBalanceViewRow> tokenRows;
};

enum class TxInstructionValueKind {
    Text,
    Address,
    TokenAmount,
};

struct TxInstructionField {
    QString label;
    QString value;
    TxInstructionValueKind kind = TxInstructionValueKind::Text;
    QString mint;
};

#endif // TXLOOKUP_H
