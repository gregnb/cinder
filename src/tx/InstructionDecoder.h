#ifndef INSTRUCTIONDECODER_H
#define INSTRUCTIONDECODER_H

#include "services/IdlRegistry.h"
#include "services/model/TransactionResponse.h"
#include "tx/AnchorIdl.h"
#include "tx/Base58.h"
#include "tx/ProgramIds.h"
#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QtEndian>

// ── Client-side instruction decoder ──────────────────────────────
//
// The Solana RPC's "jsonParsed" encoding does not parse all programs.
// Notably, ComputeBudget instructions always come back as raw base58.
// This decoder enriches unparsed Instruction objects by filling in
// program / type / info so the rendering code treats them identically
// to RPC-parsed instructions.

namespace InstructionDecoder {

    // ── ComputeBudget ────────────────────────────────────────────────

    inline bool decodeComputeBudget(Instruction& ix) {
        QByteArray raw = Base58::decode(ix.data);
        if (raw.isEmpty())
            return false;

        quint8 disc = static_cast<quint8>(raw[0]);

        if (disc == 0x00 && raw.size() == 5) {
            // RequestHeapFrame
            quint32 bytes =
                qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(raw.constData() + 1));
            ix.program = "compute-budget";
            ix.type = "requestHeapFrame";
            ix.info = QJsonObject{{"bytes", static_cast<qint64>(bytes)}};
            return true;
        }

        if (disc == 0x01 && raw.size() == 5) {
            // SetLoadedAccountsDataSizeLimit
            quint32 bytes =
                qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(raw.constData() + 1));
            ix.program = "compute-budget";
            ix.type = "setLoadedAccountsDataSizeLimit";
            ix.info = QJsonObject{{"bytes", static_cast<qint64>(bytes)}};
            return true;
        }

        if (disc == 0x02 && raw.size() == 5) {
            // SetComputeUnitLimit
            quint32 units =
                qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(raw.constData() + 1));
            ix.program = "compute-budget";
            ix.type = "setComputeUnitLimit";
            ix.info = QJsonObject{{"units", static_cast<qint64>(units)}};
            return true;
        }

        if (disc == 0x03 && raw.size() == 9) {
            // SetComputeUnitPrice
            quint64 microLamports =
                qFromLittleEndian<quint64>(reinterpret_cast<const uchar*>(raw.constData() + 1));
            ix.program = "compute-budget";
            ix.type = "setComputeUnitPrice";
            ix.info = QJsonObject{{"microLamports", static_cast<qint64>(microLamports)}};
            return true;
        }

        return false;
    }

    // ── System Program ───────────────────────────────────────────────

    inline bool decodeSystemProgram(Instruction& ix) {
        QByteArray raw = Base58::decode(ix.data);
        if (raw.size() < 4)
            return false;

        quint32 disc = qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(raw.constData()));

        if (disc == 0 && raw.size() >= 52) {
            // CreateAccount
            quint64 lamports =
                qFromLittleEndian<quint64>(reinterpret_cast<const uchar*>(raw.constData() + 4));
            quint64 space =
                qFromLittleEndian<quint64>(reinterpret_cast<const uchar*>(raw.constData() + 12));
            QString owner = Base58::encode(raw.mid(20, 32));

            ix.program = "system";
            ix.type = "createAccount";
            QJsonObject info;
            info["lamports"] = static_cast<qint64>(lamports);
            info["space"] = static_cast<qint64>(space);
            info["owner"] = owner;
            if (ix.accounts.size() >= 2) {
                info["source"] = ix.accounts[0].toString();
                info["newAccount"] = ix.accounts[1].toString();
            }
            ix.info = info;
            return true;
        }

        if (disc == 2 && raw.size() == 12) {
            // Transfer
            quint64 lamports =
                qFromLittleEndian<quint64>(reinterpret_cast<const uchar*>(raw.constData() + 4));

            ix.program = "system";
            ix.type = "transfer";
            QJsonObject info;
            info["lamports"] = static_cast<qint64>(lamports);
            if (ix.accounts.size() >= 2) {
                info["source"] = ix.accounts[0].toString();
                info["destination"] = ix.accounts[1].toString();
            }
            ix.info = info;
            return true;
        }

        if (disc == 4 && raw.size() == 4) {
            // AdvanceNonceAccount
            ix.program = "system";
            ix.type = "advanceNonce";
            QJsonObject info;
            if (ix.accounts.size() >= 3) {
                info["nonceAccount"] = ix.accounts[0].toString();
                info["nonceAuthority"] = ix.accounts[2].toString();
            }
            ix.info = info;
            return true;
        }

        return false;
    }

    // ── Anchor IDL decoding ─────────────────────────────────────────
    //
    // Uses IdlRegistry to match instruction discriminators against known
    // Anchor IDLs. Fills in program name, instruction name, and labeled
    // account names.

    inline bool decodeWithAnchor(Instruction& ix, const IdlRegistry* registry) {
        if (!registry)
            return false;

        const AnchorIdl::Idl* idl = registry->lookup(ix.programId);
        if (!idl)
            return false;

        QByteArray raw = Base58::decode(ix.data);
        if (raw.size() < 8)
            return false;

        const AnchorIdl::IdlInstruction* ixDef = idl->findInstruction(raw);
        if (!ixDef)
            return false;

        ix.program = idl->name; // e.g. "jupiter"
        ix.type = ixDef->name;  // e.g. "route_v2"

        // Label accounts using IDL account names
        QJsonObject info;
        for (int i = 0; i < ixDef->accounts.size() && i < ix.accounts.size(); ++i)
            info[ixDef->accounts[i].name] = ix.accounts[i].toString();
        ix.info = info;

        return true;
    }

    // ── Main entry point ─────────────────────────────────────────────

    inline bool tryDecode(Instruction& ix) {
        if (ix.isParsed())
            return false;
        if (ix.data.isEmpty())
            return false;

        if (ix.programId == SolanaPrograms::ComputeBudget)
            return decodeComputeBudget(ix);
        if (ix.programId == SolanaPrograms::SystemProgram)
            return decodeSystemProgram(ix);

        return false;
    }

    // Extended tryDecode with Anchor IDL support
    inline bool tryDecode(Instruction& ix, const IdlRegistry* registry) {
        if (ix.isParsed())
            return false;
        if (ix.data.isEmpty())
            return false;

        // Built-in decoders first (more precise)
        if (ix.programId == SolanaPrograms::ComputeBudget)
            return decodeComputeBudget(ix);
        if (ix.programId == SolanaPrograms::SystemProgram)
            return decodeSystemProgram(ix);

        // Anchor IDL fallback
        return decodeWithAnchor(ix, registry);
    }

    // Decode all instructions (top-level + inner) in a TransactionResponse

    inline void decodeAll(TransactionResponse& tx) {
        for (Instruction& ix : tx.message.instructions)
            tryDecode(ix);
        for (InnerInstructionSet& innerSet : tx.meta.innerInstructions)
            for (Instruction& ix : innerSet.instructions)
                tryDecode(ix);
    }

    // Extended decodeAll with Anchor IDL support
    inline void decodeAll(TransactionResponse& tx, const IdlRegistry* registry) {
        for (Instruction& ix : tx.message.instructions)
            tryDecode(ix, registry);
        for (InnerInstructionSet& innerSet : tx.meta.innerInstructions)
            for (Instruction& ix : innerSet.instructions)
                tryDecode(ix, registry);
    }

} // namespace InstructionDecoder

#endif // INSTRUCTIONDECODER_H
