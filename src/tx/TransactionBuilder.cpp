#include "TransactionBuilder.h"
#include "tx/Base58.h"
#include "tx/CompactU16.h"
#include "tx/ProgramIds.h"
#include <QMap>

TransactionBuilder::TransactionBuilder() = default;

TransactionBuilder& TransactionBuilder::setVersion(Version version) {
    m_version = version;
    return *this;
}

TransactionBuilder& TransactionBuilder::setFeePayer(const QString& pubkey) {
    m_feePayer = pubkey;
    return *this;
}

TransactionBuilder& TransactionBuilder::setRecentBlockhash(const QString& blockhash) {
    m_recentBlockhash = blockhash;
    return *this;
}

TransactionBuilder& TransactionBuilder::useNonce(const QString& noncePubkey,
                                                 const QString& authority,
                                                 const QString& nonceValue) {
    m_recentBlockhash = nonceValue;

    // Build advanceNonce instruction (SystemProgram index 4)
    TransactionInstruction advanceIx;
    advanceIx.programId = SolanaPrograms::SystemProgram;
    advanceIx.accounts = {
        AccountMeta::writable(noncePubkey, false),
        AccountMeta::readonly(SolanaPrograms::RecentBlockhashesSysvar, false),
        AccountMeta::readonly(authority, true),
    };
    QByteArray data(4, '\0');
    data[0] = 0x04;
    advanceIx.data = data;

    m_instructions.prepend(advanceIx);
    return *this;
}

TransactionBuilder& TransactionBuilder::addInstruction(const TransactionInstruction& ix) {
    m_instructions.append(ix);
    return *this;
}

QString TransactionBuilder::lastError() const { return m_lastError; }

int TransactionBuilder::numRequiredSignatures() const {
    auto msg = compile();
    return msg ? msg->numRequiredSignatures : 0;
}

// ── Compile: collect accounts, sort, build indexes ───────

std::optional<TransactionBuilder::CompiledMessage> TransactionBuilder::compile() const {
    if (m_feePayer.isEmpty()) {
        m_lastError = "Fee payer not set";
        return std::nullopt;
    }
    if (m_recentBlockhash.isEmpty()) {
        m_lastError = "Recent blockhash not set";
        return std::nullopt;
    }
    if (m_instructions.isEmpty()) {
        m_lastError = "No instructions added";
        return std::nullopt;
    }

    // Step 1: Collect unique accounts with merged flags
    struct AccountEntry {
        QString pubkey;
        bool isSigner = false;
        bool isWritable = false;
    };

    // Use ordered map to get deterministic ordering within groups
    QMap<QString, AccountEntry> accountMap;

    // Fee payer is always writable signer
    accountMap[m_feePayer] = {m_feePayer, true, true};

    for (const auto& ix : m_instructions) {
        // Program ID is readonly non-signer
        if (!accountMap.contains(ix.programId)) {
            accountMap[ix.programId] = {ix.programId, false, false};
        }

        for (const auto& meta : ix.accounts) {
            if (accountMap.contains(meta.pubkey)) {
                auto& entry = accountMap[meta.pubkey];
                entry.isSigner = entry.isSigner || meta.isSigner;
                entry.isWritable = entry.isWritable || meta.isWritable;
            } else {
                accountMap[meta.pubkey] = {meta.pubkey, meta.isSigner, meta.isWritable};
            }
        }
    }

    // Step 2: Sort into four groups
    QList<AccountEntry> writableSigners;
    QList<AccountEntry> readonlySigners;
    QList<AccountEntry> writableNonSigners;
    QList<AccountEntry> readonlyNonSigners;

    for (auto it = accountMap.cbegin(); it != accountMap.cend(); ++it) {
        const auto& entry = it.value();
        if (entry.pubkey == m_feePayer) {
            continue; // handled separately as index 0
        }

        if (entry.isSigner && entry.isWritable) {
            writableSigners.append(entry);
        } else if (entry.isSigner) {
            readonlySigners.append(entry);
        } else if (entry.isWritable) {
            writableNonSigners.append(entry);
        } else {
            readonlyNonSigners.append(entry);
        }
    }

    // Assemble: fee payer first, then groups
    QList<AccountEntry> allAccounts;
    allAccounts.append(accountMap[m_feePayer]);
    allAccounts.append(writableSigners);
    allAccounts.append(readonlySigners);
    allAccounts.append(writableNonSigners);
    allAccounts.append(readonlyNonSigners);

    // Step 3: Build header
    CompiledMessage msg;
    int totalSigners = 1 + writableSigners.size() + readonlySigners.size();
    msg.numRequiredSignatures = static_cast<uint8_t>(totalSigners);
    msg.numReadonlySignedAccounts = static_cast<uint8_t>(readonlySigners.size());
    msg.numReadonlyUnsignedAccounts = static_cast<uint8_t>(readonlyNonSigners.size());

    // Step 4: Decode all account keys to raw bytes
    QMap<QString, uint8_t> keyIndexMap;
    for (int i = 0; i < allAccounts.size(); ++i) {
        QByteArray decoded = Base58::decode(allAccounts[i].pubkey);
        if (decoded.size() != 32) {
            m_lastError = QString("Invalid pubkey: %1").arg(allAccounts[i].pubkey);
            return std::nullopt;
        }
        msg.accountKeys.append(decoded);
        keyIndexMap[allAccounts[i].pubkey] = static_cast<uint8_t>(i);
    }

    // Step 5: Decode blockhash
    msg.recentBlockhash = Base58::decode(m_recentBlockhash);
    if (msg.recentBlockhash.size() != 32) {
        m_lastError = "Invalid blockhash";
        return std::nullopt;
    }

    // Step 6: Compile instructions
    for (const auto& ix : m_instructions) {
        CompiledMessage::CompiledInstruction cix;

        if (!keyIndexMap.contains(ix.programId)) {
            m_lastError = QString("Program ID not in account list: %1").arg(ix.programId);
            return std::nullopt;
        }
        cix.programIdIndex = keyIndexMap[ix.programId];

        for (const auto& meta : ix.accounts) {
            if (!keyIndexMap.contains(meta.pubkey)) {
                m_lastError = QString("Account not in key list: %1").arg(meta.pubkey);
                return std::nullopt;
            }
            cix.accountIndexes.append(keyIndexMap[meta.pubkey]);
        }

        cix.data = ix.data;
        msg.instructions.append(cix);
    }

    return msg;
}

// ── Serialize compiled message to wire format ────────────

QByteArray TransactionBuilder::serializeCompiled(const CompiledMessage& msg) const {
    QByteArray out;
    out.reserve(512);

    // V0 prefix
    if (m_version == Version::V0) {
        out.append(static_cast<char>(0x80));
    }

    // Header: 3 bytes
    out.append(static_cast<char>(msg.numRequiredSignatures));
    out.append(static_cast<char>(msg.numReadonlySignedAccounts));
    out.append(static_cast<char>(msg.numReadonlyUnsignedAccounts));

    // Account keys
    CompactU16::encode(static_cast<uint16_t>(msg.accountKeys.size()), out);
    for (const auto& key : msg.accountKeys) {
        out.append(key);
    }

    // Recent blockhash
    out.append(msg.recentBlockhash);

    // Instructions
    CompactU16::encode(static_cast<uint16_t>(msg.instructions.size()), out);
    for (const auto& cix : msg.instructions) {
        out.append(static_cast<char>(cix.programIdIndex));

        CompactU16::encode(static_cast<uint16_t>(cix.accountIndexes.size()), out);
        for (uint8_t idx : cix.accountIndexes) {
            out.append(static_cast<char>(idx));
        }

        CompactU16::encode(static_cast<uint16_t>(cix.data.size()), out);
        out.append(cix.data);
    }

    // V0: address table lookups (none for now)
    if (m_version == Version::V0) {
        CompactU16::encode(0, out);
    }

    return out;
}

QByteArray TransactionBuilder::serializeMessage() const {
    auto msg = compile();
    if (!msg) {
        return {};
    }
    return serializeCompiled(*msg);
}

QByteArray TransactionBuilder::buildSigned(const QList<QByteArray>& signatures) const {
    auto msg = compile();
    if (!msg) {
        return {};
    }

    if (signatures.size() != msg->numRequiredSignatures) {
        m_lastError = QString("Expected %1 signatures, got %2")
                          .arg(msg->numRequiredSignatures)
                          .arg(signatures.size());
        return {};
    }

    for (int i = 0; i < signatures.size(); ++i) {
        if (signatures[i].size() != 64) {
            m_lastError =
                QString("Signature %1 is %2 bytes, expected 64").arg(i).arg(signatures[i].size());
            return {};
        }
    }

    QByteArray out;
    out.reserve(1232);

    // Signature count + signatures
    CompactU16::encode(static_cast<uint16_t>(signatures.size()), out);
    for (const auto& sig : signatures) {
        out.append(sig);
    }

    // Message bytes
    out.append(serializeCompiled(*msg));

    return out;
}
