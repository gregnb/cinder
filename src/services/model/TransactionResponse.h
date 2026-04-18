#ifndef TRANSACTIONRESPONSE_H
#define TRANSACTIONRESPONSE_H

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>

// ── Sub-models ────────────────────────────────────────────

struct AccountKey {
    QString pubkey;
    bool signer = false;
    bool writable = false;
    QString source; // "transaction" or "lookupTable"

    static AccountKey fromJson(const QJsonObject& json) {
        return {json["pubkey"].toString(), json["signer"].toBool(), json["writable"].toBool(),
                json["source"].toString()};
    }
};

struct TokenAmount {
    QString amount; // raw u64 string
    int decimals = 0;
    double uiAmount = 0;
    QString uiAmountString;

    static TokenAmount fromJson(const QJsonObject& json) {
        return {json["amount"].toString(), json["decimals"].toInt(), json["uiAmount"].toDouble(),
                json["uiAmountString"].toString()};
    }
};

struct TokenBalance {
    int accountIndex = 0;
    QString mint;
    QString owner;
    QString programId; // TokenProgram or Token2022Program
    TokenAmount amount;

    static TokenBalance fromJson(const QJsonObject& json) {
        TokenBalance tb;
        tb.accountIndex = json["accountIndex"].toInt();
        tb.mint = json["mint"].toString();
        tb.owner = json["owner"].toString();
        tb.programId = json["programId"].toString();
        tb.amount = TokenAmount::fromJson(json["uiTokenAmount"].toObject());
        return tb;
    }
};

struct Instruction {
    QString programId;
    QString program;  // "system", "spl-token", etc. (empty if unparsed)
    QString type;     // "transfer", "advanceNonce", etc.
    QJsonObject info; // parsed info (program-specific, kept as JSON)
    int stackHeight = 0;

    // Unparsed fallback
    QString data;        // base58 encoded
    QJsonArray accounts; // array of pubkey strings

    bool isParsed() const { return !program.isEmpty(); }

    static Instruction fromJson(const QJsonObject& json) {
        Instruction ix;
        ix.programId = json["programId"].toString();
        ix.program = json["program"].toString();
        ix.stackHeight = json["stackHeight"].toInt();

        if (json.contains("parsed")) {
            const QJsonObject parsed = json["parsed"].toObject();
            ix.type = parsed["type"].toString();
            ix.info = parsed["info"].toObject();
        } else {
            ix.data = json["data"].toString();
            ix.accounts = json["accounts"].toArray();
        }
        return ix;
    }
};

struct InnerInstructionSet {
    int index = 0; // which top-level instruction spawned these
    QList<Instruction> instructions;

    static InnerInstructionSet fromJson(const QJsonObject& json) {
        InnerInstructionSet set;
        set.index = json["index"].toInt();
        for (const auto& v : json["instructions"].toArray())
            set.instructions.append(Instruction::fromJson(v.toObject()));
        return set;
    }
};

struct AddressTableLookup {
    QString accountKey; // lookup table address
    QList<int> writableIndexes;
    QList<int> readonlyIndexes;

    static AddressTableLookup fromJson(const QJsonObject& json) {
        AddressTableLookup atl;
        atl.accountKey = json["accountKey"].toString();
        for (const auto& v : json["writableIndexes"].toArray())
            atl.writableIndexes.append(v.toInt());
        for (const auto& v : json["readonlyIndexes"].toArray())
            atl.readonlyIndexes.append(v.toInt());
        return atl;
    }
};

struct LoadedAddresses {
    QStringList readonly;
    QStringList writable;

    static LoadedAddresses fromJson(const QJsonObject& json) {
        LoadedAddresses la;
        for (const auto& v : json["readonly"].toArray())
            la.readonly.append(v.toString());
        for (const auto& v : json["writable"].toArray())
            la.writable.append(v.toString());
        return la;
    }
};

struct ReturnData {
    QString programId;
    QByteArray data; // decoded from base64

    static ReturnData fromJson(const QJsonObject& json) {
        ReturnData rd;
        rd.programId = json["programId"].toString();
        const QJsonArray arr = json["data"].toArray();
        if (arr.size() >= 1)
            rd.data = QByteArray::fromBase64(arr[0].toString().toUtf8());
        return rd;
    }
};

struct RewardInfo {
    QString pubkey;
    qint64 lamports = 0;
    qint64 postBalance = 0;
    QString rewardType;
    int commission = -1;

    static RewardInfo fromJson(const QJsonObject& json) {
        RewardInfo reward;
        reward.pubkey = json["pubkey"].toString();
        reward.lamports = json["lamports"].toInteger();
        reward.postBalance = json["postBalance"].toInteger();
        reward.rewardType = json["rewardType"].toString();
        if (json.contains("commission") && !json["commission"].isNull()) {
            reward.commission = json["commission"].toInt(-1);
        }
        return reward;
    }
};

// ── Meta ──────────────────────────────────────────────────

struct TransactionMeta {
    qint64 fee = 0;
    qint64 computeUnitsConsumed = 0;
    bool hasError = false;
    QJsonValue err; // null or error object
    QList<qint64> preBalances;
    QList<qint64> postBalances;
    QList<TokenBalance> preTokenBalances;
    QList<TokenBalance> postTokenBalances;
    QList<InnerInstructionSet> innerInstructions;
    QStringList logMessages;
    LoadedAddresses loadedAddresses; // v0 transactions only
    ReturnData returnData;
    QList<RewardInfo> rewards;

    static TransactionMeta fromJson(const QJsonObject& json) {
        TransactionMeta m;
        m.fee = json["fee"].toInteger();
        m.computeUnitsConsumed = json["computeUnitsConsumed"].toInteger();
        m.err = json["err"];
        m.hasError = !json["err"].isNull();

        for (const auto& v : json["preBalances"].toArray())
            m.preBalances.append(v.toInteger());
        for (const auto& v : json["postBalances"].toArray())
            m.postBalances.append(v.toInteger());
        for (const auto& v : json["preTokenBalances"].toArray())
            m.preTokenBalances.append(TokenBalance::fromJson(v.toObject()));
        for (const auto& v : json["postTokenBalances"].toArray())
            m.postTokenBalances.append(TokenBalance::fromJson(v.toObject()));
        for (const auto& v : json["innerInstructions"].toArray())
            m.innerInstructions.append(InnerInstructionSet::fromJson(v.toObject()));
        for (const auto& v : json["logMessages"].toArray())
            m.logMessages.append(v.toString());
        for (const auto& v : json["rewards"].toArray())
            m.rewards.append(RewardInfo::fromJson(v.toObject()));

        if (json.contains("loadedAddresses"))
            m.loadedAddresses = LoadedAddresses::fromJson(json["loadedAddresses"].toObject());
        if (json.contains("returnData") && !json["returnData"].isNull())
            m.returnData = ReturnData::fromJson(json["returnData"].toObject());

        return m;
    }
};

// ── Message ───────────────────────────────────────────────

struct TransactionMessage {
    QList<AccountKey> accountKeys;
    QList<Instruction> instructions;
    QString recentBlockhash;
    QList<AddressTableLookup> addressTableLookups; // v0 transactions only

    static TransactionMessage fromJson(const QJsonObject& json) {
        TransactionMessage msg;
        msg.recentBlockhash = json["recentBlockhash"].toString();
        for (const auto& v : json["accountKeys"].toArray())
            msg.accountKeys.append(AccountKey::fromJson(v.toObject()));
        for (const auto& v : json["instructions"].toArray())
            msg.instructions.append(Instruction::fromJson(v.toObject()));
        if (json.contains("addressTableLookups")) {
            for (const auto& v : json["addressTableLookups"].toArray())
                msg.addressTableLookups.append(AddressTableLookup::fromJson(v.toObject()));
        }
        return msg;
    }
};

// ── Top-level response ────────────────────────────────────

struct TransactionResponse {
    qint64 slot = 0;
    qint64 blockTime = 0;
    QString version; // "legacy" or "0"
    QStringList signatures;
    TransactionMessage message;
    TransactionMeta meta;
    QJsonObject rawJson; // full JSON-RPC result for DB storage

    // Helper: find the account index for a given pubkey
    int accountIndex(const QString& pubkey) const {
        for (int i = 0; i < message.accountKeys.size(); ++i) {
            if (message.accountKeys[i].pubkey == pubkey) {
                return i;
            }
        }
        return -1;
    }

    // Helper: SOL balance change for an account (in lamports)
    qint64 solBalanceChange(int index) const {
        if (index < 0 || index >= meta.preBalances.size()) {
            return 0;
        }
        return meta.postBalances[index] - meta.preBalances[index];
    }

    static TransactionResponse fromJson(const QJsonObject& json) {
        TransactionResponse tx;
        tx.rawJson = json;
        tx.slot = json["slot"].toInteger();
        tx.blockTime = json["blockTime"].toInteger();

        // version can be string "legacy" or number 0
        if (json["version"].isDouble()) {
            tx.version = QString::number(json["version"].toInt());
        } else {
            tx.version = json["version"].toString("legacy");
        }

        tx.meta = TransactionMeta::fromJson(json["meta"].toObject());

        const QJsonObject txObj = json["transaction"].toObject();
        for (const auto& v : txObj["signatures"].toArray())
            tx.signatures.append(v.toString());
        tx.message = TransactionMessage::fromJson(txObj["message"].toObject());

        return tx;
    }
};

Q_DECLARE_METATYPE(TransactionResponse)

#endif // TRANSACTIONRESPONSE_H
