#include "features/txlookup/TxLookupHandler.h"

#include "db/TransactionDb.h"
#include "services/IdlRegistry.h"
#include "services/SolanaApi.h"
#include "services/model/TransactionResponse.h"
#include "tx/Base58.h"
#include "tx/ProgramIds.h"
#include "tx/TxClassifier.h"
#include "tx/TxParseUtils.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLocale>
#include <QMap>
#include <cmath>

TxLookupHandler::TxLookupHandler(SolanaApi* api, QObject* parent)
    : QObject(parent), m_solanaApi(api) {}

void TxLookupHandler::setWalletAddress(const QString& address) { m_walletAddress = address; }

void TxLookupHandler::loadTransaction(const QString& signature) {
    const QString rawJson = TransactionDb::getRawJson(signature);
    if (!rawJson.isEmpty()) {
        const QJsonDocument doc = QJsonDocument::fromJson(rawJson.toUtf8());
        const TransactionResponse tx = TransactionResponse::fromJson(doc.object());
        emit transactionLoaded(signature, tx);
        return;
    }

    if (!m_solanaApi) {
        emit transactionLoadFailed(signature, LoadError::NoLocalNoApi, QString());
        return;
    }

    disconnect(m_txConn);
    disconnect(m_errConn);
    m_pendingSig = signature;

    m_txConn = connect(m_solanaApi, &SolanaApi::transactionReady, this,
                       [this](const QString& sig, const TransactionResponse& tx) {
                           if (sig != m_pendingSig) {
                               return;
                           }

                           disconnect(m_txConn);
                           disconnect(m_errConn);
                           m_pendingSig.clear();
                           const QString rawJson =
                               QJsonDocument(tx.rawJson).toJson(QJsonDocument::Compact);
                           const QList<Activity> activities =
                               m_walletAddress.isEmpty()
                                   ? QList<Activity>{}
                                   : TxParseUtils::extractActivities(tx, m_walletAddress);
                           TransactionDb::insertTransaction(sig, tx.slot, tx.blockTime, rawJson,
                                                            static_cast<int>(tx.meta.fee),
                                                            tx.meta.hasError, activities);
                           emit transactionLoaded(sig, tx);
                       });

    m_errConn = connect(m_solanaApi, &SolanaApi::requestFailed, this,
                        [this](const QString& method, const QString& error) {
                            if (method != QLatin1String("getTransaction")) {
                                return;
                            }

                            const QString failedSig = m_pendingSig;
                            disconnect(m_txConn);
                            disconnect(m_errConn);
                            m_pendingSig.clear();
                            emit transactionLoadFailed(failedSig, LoadError::FetchFailed, error);
                        });

    m_solanaApi->fetchTransaction(signature);
}

bool TxLookupHandler::isValidSignature(const QString& text) {
    if (text.length() < 80 || text.length() > 90) {
        return false;
    }

    const QByteArray decoded = Base58::decode(text);
    return decoded.size() == 64;
}

QString TxLookupHandler::firstSigner(const TransactionResponse& tx) const {
    for (const auto& accountKey : tx.message.accountKeys) {
        if (accountKey.signer) {
            return accountKey.pubkey;
        }
    }

    return QString();
}

qint64 TxLookupHandler::priorityFeeLamports(const TransactionResponse& tx) const {
    int signerCount = 0;
    for (const auto& accountKey : tx.message.accountKeys) {
        if (accountKey.signer) {
            ++signerCount;
        }
    }

    const qint64 baseFeeLamports = signerCount * 5000LL;
    return tx.meta.fee - baseFeeLamports;
}

bool TxLookupHandler::usesDurableNonce(const TransactionResponse& tx) const {
    if (tx.message.instructions.isEmpty()) {
        return false;
    }

    return tx.message.instructions.first().type == QLatin1String("advanceNonce");
}

TxClassifier::Classification TxLookupHandler::classify(const TransactionResponse& tx,
                                                       const QString& walletAddress) const {
    return TxClassifier::classify(tx, walletAddress);
}

QList<TxParseUtils::CuEntry>
TxLookupHandler::computeUnitEntries(const TransactionResponse& tx) const {
    return TxParseUtils::parseCuBreakdown(tx.meta.logMessages,
                                          static_cast<quint64>(tx.meta.computeUnitsConsumed));
}

TxBalanceChanges TxLookupHandler::balanceChanges(const TransactionResponse& tx) const {
    TxBalanceChanges result;

    const int accountCount = qMin(tx.message.accountKeys.size(), tx.meta.preBalances.size());
    for (int i = 0; i < accountCount; ++i) {
        const qint64 pre = tx.meta.preBalances[i];
        const qint64 post = (i < tx.meta.postBalances.size()) ? tx.meta.postBalances[i] : pre;
        const qint64 delta = post - pre;
        if (delta == 0) {
            continue;
        }

        SolBalanceChange change;
        change.address = tx.message.accountKeys[i].pubkey;
        change.preLamports = pre;
        change.postLamports = post;
        change.deltaLamports = delta;
        result.solChanges.append(change);
    }

    struct TokenEntry {
        QString owner;
        QString mint;
        double preAmount = 0.0;
        double postAmount = 0.0;
    };

    QMap<QString, TokenEntry> tokenMap;
    for (const auto& tb : tx.meta.preTokenBalances) {
        const QString key = QString::number(tb.accountIndex) + ":" + tb.mint;
        tokenMap[key].owner = tb.owner;
        tokenMap[key].mint = tb.mint;
        tokenMap[key].preAmount = tb.amount.uiAmount;
    }
    for (const auto& tb : tx.meta.postTokenBalances) {
        const QString key = QString::number(tb.accountIndex) + ":" + tb.mint;
        if (tokenMap[key].owner.isEmpty()) {
            tokenMap[key].owner = tb.owner;
        }
        tokenMap[key].mint = tb.mint;
        tokenMap[key].postAmount = tb.amount.uiAmount;
    }

    for (auto it = tokenMap.constBegin(); it != tokenMap.constEnd(); ++it) {
        const double delta = it.value().postAmount - it.value().preAmount;
        if (delta == 0.0) {
            continue;
        }

        TokenBalanceChange change;
        change.owner = it.value().owner;
        change.mint = it.value().mint;
        change.preAmount = it.value().preAmount;
        change.postAmount = it.value().postAmount;
        change.deltaAmount = delta;
        result.tokenChanges.append(change);
    }

    return result;
}

TxBalanceViewData TxLookupHandler::balanceViewData(const TransactionResponse& tx) const {
    const TxBalanceChanges changes = balanceChanges(tx);
    TxBalanceViewData viewData;
    const QLocale locale;

    for (const SolBalanceChange& change : changes.solChanges) {
        const double preSol = change.preLamports / 1e9;
        const double postSol = change.postLamports / 1e9;
        const double deltaSol = change.deltaLamports / 1e9;

        TxBalanceViewRow row;
        row.address = change.address;
        row.beforeText = locale.toString(preSol, 'f', 6);
        row.afterText = locale.toString(postSol, 'f', 6);
        row.changeText = (deltaSol > 0 ? "+" : "") + locale.toString(deltaSol, 'f', 6);
        row.isPositiveChange = deltaSol > 0;
        row.mint = WSOL_MINT;
        row.isNativeSol = true;
        viewData.solRows.append(row);
    }

    for (const TokenBalanceChange& change : changes.tokenChanges) {
        const double absDelta = std::abs(change.deltaAmount);

        TxBalanceViewRow row;
        row.address = change.owner;
        row.beforeText = locale.toString(change.preAmount, 'f', change.preAmount >= 1 ? 6 : 10);
        row.afterText = locale.toString(change.postAmount, 'f', change.postAmount >= 1 ? 6 : 10);
        row.changeText = (change.deltaAmount > 0 ? "+" : "") +
                         locale.toString(change.deltaAmount, 'f', absDelta >= 1 ? 6 : 10);
        row.isPositiveChange = change.deltaAmount > 0;
        row.mint = change.mint;
        viewData.tokenRows.append(row);
    }

    return viewData;
}

QString TxLookupHandler::friendlyProgramName(const QString& programId,
                                             const IdlRegistry* registry) const {
    if (registry) {
        const QString name = registry->friendlyName(programId);
        if (!name.contains("...")) {
            return name;
        }
    }
    if (programId == SolanaPrograms::SystemProgram) {
        return "System Program";
    }
    if (programId == SolanaPrograms::ComputeBudget) {
        return "Compute Budget Program";
    }
    if (programId == SolanaPrograms::TokenProgram) {
        return "Token Program";
    }
    if (programId == SolanaPrograms::Token2022Program) {
        return "Token-2022 Program";
    }
    if (programId == SolanaPrograms::AssociatedTokenAccount) {
        return "Associated Token Program";
    }
    if (programId == SolanaPrograms::MemoProgram) {
        return "Memo Program";
    }
    if (programId == SolanaPrograms::StakeProgram) {
        return "Stake Program";
    }
    if (programId == SolanaPrograms::VoteProgram) {
        return "Vote Program";
    }
    if (programId == SolanaPrograms::BpfLoader) {
        return "BPF Loader";
    }
    if (programId == SolanaPrograms::BpfUpgradeableLoader) {
        return "BPF Upgradeable Loader";
    }
    // Check DEX program registry
    const QString dex = TxClassifier::dexName(programId);
    if (!dex.contains("...")) {
        return dex;
    }
    // Well-known third-party programs
    static const QMap<QString, QString> thirdParty = {
        {"DeJBGdMFa1uynnnKiwrVioatTuHmNLpyFKnmB5kaFdzQ", "Phantom Transfer"},
        {"MemoSq4gqABAXKb96qnH8TysNcWxMyWCqXgDLGmfcHr", "Memo v2"},
        {"Memo1UhkJBfCR961jRFg2HJBrSt9kgSCcLGsMVq8Fg", "Memo v1"},
        {"namesLPneVptA9Z5rqUDD9tMTWEJwofgaYwp8cawRkX", "Name Service"},
        {"metaqbxxUerdq28cj1RbAWkYQm3ybzjb6a8bt518x1s", "Metaplex Token Metadata"},
        {"auth9SigNpDKz4sJJ1DfCTuZrZNSAgh9sFD3rboVmgg", "Metaplex Authorization"},
        {"ATokenGPvbdGVxr1b2hvZbsiqW5xWH25efTNsLJA8knL", "Associated Token Program"},
        {"cndy3Z4yapfJBmearMSSr1sv1owVnJ4hgpBCMi7bjr9", "Candy Machine v2"},
        {"Guard1JwRhJkVH6XZhzoYxeBVQe872VH6QggF4BWmS9g", "Candy Guard"},
    };
    auto it = thirdParty.find(programId);
    if (it != thirdParty.end()) {
        return it.value();
    }
    return programId.left(8) + "..." + programId.right(4);
}

QString TxLookupHandler::formatTypeName(const QString& type) const {
    if (type.isEmpty()) {
        return "Unknown";
    }

    QString result;
    bool nextUpper = true;
    for (int i = 0; i < type.length(); ++i) {
        if (type[i] == '_') {
            result += ' ';
            nextUpper = true;
        } else if (nextUpper) {
            result += type[i].toUpper();
            nextUpper = false;
        } else if (type[i].isUpper()) {
            result += ' ';
            result += type[i];
        } else {
            result += type[i];
        }
    }
    return result;
}

QString TxLookupHandler::formatKeyName(const QString& key) const {
    if (key.isEmpty()) {
        return key;
    }

    QString result;
    for (int i = 0; i < key.length(); ++i) {
        if (i == 0) {
            result += key[i].toUpper();
        } else if (key[i].isUpper()) {
            result += ' ';
            result += key[i];
        } else {
            result += key[i];
        }
    }
    return result;
}

bool TxLookupHandler::looksLikeAddress(const QString& text) const {
    if (text.length() < 32 || text.length() > 44) {
        return false;
    }

    static const QString base58Chars = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
    for (const QChar& ch : text) {
        if (!base58Chars.contains(ch)) {
            return false;
        }
    }
    return true;
}

QMap<int, QList<Instruction>>
TxLookupHandler::innerInstructionsByIndex(const TransactionResponse& tx) const {
    QMap<int, QList<Instruction>> innerMap;
    for (const auto& innerSet : tx.meta.innerInstructions) {
        innerMap[innerSet.index] = innerSet.instructions;
    }
    return innerMap;
}

QMap<QString, AccountKey>
TxLookupHandler::accountKeyByAddress(const TransactionResponse& tx) const {
    QMap<QString, AccountKey> accountKeyMap;
    for (const auto& accountKey : tx.message.accountKeys) {
        accountKeyMap[accountKey.pubkey] = accountKey;
    }
    return accountKeyMap;
}

QString TxLookupHandler::feePayerAddress(const TransactionResponse& tx) const {
    for (const auto& accountKey : tx.message.accountKeys) {
        if (accountKey.signer) {
            return accountKey.pubkey;
        }
    }
    return QString();
}

QList<TxInstructionField> TxLookupHandler::instructionFields(const Instruction& ix) const {
    QList<TxInstructionField> fields;
    if (!ix.isParsed()) {
        return fields;
    }

    const QString mint = ix.info.value("mint").toString();
    for (const QString& key : ix.info.keys()) {
        const QJsonValue value = ix.info[key];
        TxInstructionField field;
        field.label = formatKeyName(key);

        if (value.isString()) {
            field.value = value.toString();
            field.kind = looksLikeAddress(field.value) ? TxInstructionValueKind::Address
                                                       : TxInstructionValueKind::Text;
            fields.append(field);
            continue;
        }

        if (value.isDouble()) {
            const double numeric = value.toDouble();
            if (key == "microLamports") {
                const qlonglong microLamports = static_cast<qlonglong>(numeric);
                field.value = QLocale().toString(microLamports) + " micro-lamports";
            } else if (key.toLower().contains("lamport")) {
                const qlonglong lamports = static_cast<qlonglong>(numeric);
                const double sol = lamports / 1e9;
                field.value = QString::number(sol, 'f', sol < 0.001 ? 9 : 6) + " SOL";
            } else {
                const qlonglong asInt = static_cast<qlonglong>(numeric);
                if (static_cast<double>(asInt) == numeric) {
                    field.value = QLocale().toString(asInt);
                } else {
                    field.value = QString::number(numeric, 'g', 10);
                }
            }
            fields.append(field);
            continue;
        }

        if (value.isObject()) {
            const QJsonObject obj = value.toObject();
            if (obj.contains("uiAmountString")) {
                field.value = obj["uiAmountString"].toString();
                if (!mint.isEmpty()) {
                    field.kind = TxInstructionValueKind::TokenAmount;
                    field.mint = mint;
                }
            } else {
                field.value = QJsonDocument(obj).toJson(QJsonDocument::Compact);
            }
            fields.append(field);
            continue;
        }

        if (value.isBool()) {
            field.value = value.toBool() ? "true" : "false";
            fields.append(field);
        }
    }

    return fields;
}
