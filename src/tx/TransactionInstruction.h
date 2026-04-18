#ifndef TRANSACTIONINSTRUCTION_H
#define TRANSACTIONINSTRUCTION_H

#include "tx/AccountMeta.h"
#include <QByteArray>
#include <QList>
#include <QString>

struct TransactionInstruction {
    QString programId;
    QList<AccountMeta> accounts;
    QByteArray data;
};

#endif // TRANSACTIONINSTRUCTION_H
