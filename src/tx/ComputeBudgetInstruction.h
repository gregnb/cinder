#ifndef COMPUTEBUDGETINSTRUCTION_H
#define COMPUTEBUDGETINSTRUCTION_H

#include "tx/ProgramIds.h"
#include "tx/TransactionInstruction.h"
#include <QByteArray>
#include <QtEndian>

namespace ComputeBudgetInstruction {

    // SetComputeUnitLimit (discriminator 2)
    inline TransactionInstruction setComputeUnitLimit(quint32 units) {
        TransactionInstruction ix;
        ix.programId = SolanaPrograms::ComputeBudget;
        QByteArray data(1, '\x02');
        QByteArray val(4, '\0');
        qToLittleEndian(units, reinterpret_cast<uchar*>(val.data()));
        ix.data = data + val;
        return ix;
    }

    // SetComputeUnitPrice (discriminator 3)
    inline TransactionInstruction setComputeUnitPrice(quint64 microLamports) {
        TransactionInstruction ix;
        ix.programId = SolanaPrograms::ComputeBudget;
        QByteArray data(1, '\x03');
        QByteArray val(8, '\0');
        qToLittleEndian(microLamports, reinterpret_cast<uchar*>(val.data()));
        ix.data = data + val;
        return ix;
    }

} // namespace ComputeBudgetInstruction

#endif // COMPUTEBUDGETINSTRUCTION_H
