#include "Constants.h"
#include "TerminalHandler.h"
#include "TerminalHandlerCommon.h"

#include "crypto/HDDerivation.h"
#include "crypto/Mnemonic.h"
#include "db/ContactDb.h"
#include "db/IdlDb.h"
#include "db/PortfolioDb.h"
#include "db/TokenAccountDb.h"
#include "db/TransactionDb.h"
#include "db/WalletDb.h"
#include "services/IdlRegistry.h"
#include "services/SolanaApi.h"
#include "tx/AnchorIdl.h"
#include "tx/Base58.h"
#include "tx/KnownTokens.h"
#include "tx/TxClassifier.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QMap>
#include <limits>
#include <sodium.h>

using namespace terminal;

void TerminalHandler::cmdVersion() {
    emitOutput(QCoreApplication::translate("TerminalPage", "Cinder v%1").arg(AppVersion::string));
    emitOutput(
        QCoreApplication::translate("TerminalPage", "Built with Qt %1").arg(QString(qVersion())),
        kDimColor);
}

void TerminalHandler::cmdWallet(const TerminalParsedCommand& cmd) {
    switch (cmd.sub) {
        case TerminalSubcommand::None:
        case TerminalSubcommand::WalletInfo: {
            auto wallets = WalletDb::getAllRecords();
            if (wallets.isEmpty()) {
                emitOutput("No wallet loaded.", kErrorColor);
                break;
            }
            auto w = WalletDb::getByAddressRecord(m_walletAddress);
            if (!w) {
                w = WalletDb::getByAddressRecord(wallets.first().address);
            }
            if (!w) {
                emitOutput("No wallet loaded.", kErrorColor);
                break;
            }
            emitOutput("  Address:   " + w->address);
            emitOutput("  Label:     " + w->label, kDimColor);
            emitOutput("  Key Type:  " + w->keyType, kDimColor);
            emitOutput("  RPC:       " + m_api->rpcUrl(), kDimColor);
            break;
        }
        default:
            break;
    }
}

void TerminalHandler::cmdKey(const TerminalParsedCommand& cmd) {
    const auto& args = cmd.parts;

    switch (cmd.sub) {
        case TerminalSubcommand::None:
            emitOutput("Usage: key <generate|from-secret|from-mnemonic|pubkey|verify>", kDimColor);
            break;
        case TerminalSubcommand::KeyGenerate: {
            Keypair kp = Keypair::generate();
            emitOutput("  Address:  " + kp.address(), kPromptColor);
            QString secret = kp.toBase58();
            if (m_secretSink) {
                m_secretSink("Secret", secret);
            } else {
                emitOutput("  Secret:   " + secret, kWarnColor);
            }
            wipeQString(secret);
            break;
        }
        case TerminalSubcommand::KeyFromSecret: {
            if (args.size() < 3) {
                emitOutput("Usage: key from-secret <base58>", kDimColor);
                break;
            }
            Keypair kp = Keypair::fromBase58(args[2]);
            if (kp.isNull()) {
                emitOutput("Invalid secret key.", kErrorColor);
                break;
            }
            emitOutput("  Address:  " + kp.address(), kPromptColor);
            break;
        }
        case TerminalSubcommand::KeyFromMnemonic: {
            if (args.size() < 14) {
                emitOutput("Usage: key from-mnemonic <12 or 24 words> [--path m/44'/501'/0'/0']",
                           kDimColor);
                break;
            }
            QStringList words;
            QString path = "m/44'/501'/0'/0'";
            for (int i = 2; i < args.size(); ++i) {
                if (args[i] == "--path" && i + 1 < args.size()) {
                    path = args[++i];
                } else {
                    words << args[i];
                }
            }
            QString mnemonic = words.join(' ');
            if (!Mnemonic::validate(mnemonic)) {
                emitOutput("Invalid mnemonic.", kErrorColor);
                break;
            }
            QByteArray seed = Mnemonic::toSeed(mnemonic);
            QByteArray derived = HDDerivation::derive(seed, path);
            Keypair kp = Keypair::fromSeed(derived);
            emitOutput("  Path:     " + path, kDimColor);
            emitOutput("  Address:  " + kp.address(), kPromptColor);
            QString secret = kp.toBase58();
            if (m_secretSink) {
                m_secretSink("Secret", secret);
            } else {
                emitOutput("  Secret:   " + secret, kWarnColor);
            }
            wipeQString(secret);
            wipeQString(mnemonic);
            sodium_memzero(seed.data(), static_cast<size_t>(seed.size()));
            sodium_memzero(derived.data(), static_cast<size_t>(derived.size()));
            break;
        }
        case TerminalSubcommand::KeyPubkey: {
            if (args.size() < 3) {
                emitOutput("Usage: key pubkey <address>", kDimColor);
                break;
            }
            QByteArray decoded = Base58::decode(args[2]);
            if (decoded.size() == 32) {
                emitOutput("  \xe2\x9c\x93 Valid Ed25519 public key (32 bytes)", kPromptColor);
            } else {
                emitOutput("  \xe2\x9c\x97 Invalid: decoded to " + QString::number(decoded.size()) +
                               " bytes (expected 32)",
                           kErrorColor);
            }
            break;
        }
        case TerminalSubcommand::KeyVerify: {
            if (args.size() < 5) {
                emitOutput("Usage: key verify <pubkey> <message_hex> <signature_hex>", kDimColor);
                break;
            }
            QByteArray pub = Base58::decode(args[2]);
            QByteArray msg = QByteArray::fromHex(args[3].toLatin1());
            QByteArray sig = QByteArray::fromHex(args[4].toLatin1());
            if (Keypair::verify(pub, msg, sig)) {
                emitOutput("  \xe2\x9c\x93 Signature valid", kPromptColor);
            } else {
                emitOutput("  \xe2\x9c\x97 Signature invalid", kErrorColor);
            }
            break;
        }
        default:
            break;
    }
}

void TerminalHandler::cmdMnemonic(const TerminalParsedCommand& cmd) {
    const auto& args = cmd.parts;

    switch (cmd.sub) {
        case TerminalSubcommand::None:
            emitOutput("Usage: mnemonic <generate|validate>", kDimColor);
            break;
        case TerminalSubcommand::MnemonicGenerate: {
            int count = 12;
            if (args.size() > 2) {
                count = args[2].toInt();
            }
            if (count != 12 && count != 24) {
                count = 12;
            }
            QString phrase = Mnemonic::generate(count);
            if (m_secretSink) {
                m_secretSink("Mnemonic", phrase);
            } else {
                emitOutput("  Mnemonic: " + phrase, kWarnColor);
            }
            wipeQString(phrase);
            break;
        }
        case TerminalSubcommand::MnemonicValidate: {
            if (args.size() < 4) {
                emitOutput("Usage: mnemonic validate <word1 word2 ...>", kDimColor);
                break;
            }
            QStringList words = args.mid(2);
            QString phrase = words.join(' ');
            if (Mnemonic::validate(phrase)) {
                emitOutput("  \xe2\x9c\x93 Valid BIP39 mnemonic (" + QString::number(words.size()) +
                               " words)",
                           kPromptColor);
            } else {
                emitOutput("  \xe2\x9c\x97 Invalid mnemonic", kErrorColor);
            }
            break;
        }
        default:
            break;
    }
}

void TerminalHandler::cmdContact(const TerminalParsedCommand& cmd) {
    const auto& args = cmd.parts;

    switch (cmd.sub) {
        case TerminalSubcommand::None:
            emitOutput("Usage: contact <list|add|remove|find|lookup>", kDimColor);
            break;
        case TerminalSubcommand::ContactList: {
            auto contacts = ContactDb::getAllRecords();
            if (contacts.isEmpty()) {
                emitOutput("  No contacts saved.", kDimColor);
                break;
            }
            emitOutput(padRight("  ID", 6) + padRight("Name", 20) + "Address", kDimColor);
            emitOutput("  " + QString(44, QChar(0x2500)), kDimColor);
            for (const auto& c : contacts) {
                emitOutput("  " + padRight(QString::number(c.id), 4) + padRight(c.name, 20) +
                           truncAddr(c.address));
            }
            break;
        }
        case TerminalSubcommand::ContactAdd: {
            if (args.size() < 4) {
                emitOutput("Usage: contact add <name> <address>", kDimColor);
                break;
            }
            const QString& address = args.last();
            QStringList nameWords = args.mid(2, args.size() - 3);
            QString name = nameWords.join(' ');
            if (ContactDb::insertContact(name, address)) {
                emitOutput("  \xe2\x9c\x93 Contact added: " + name, kPromptColor);
            } else {
                emitOutput("  Failed to add contact.", kErrorColor);
            }
            break;
        }
        case TerminalSubcommand::ContactRemove: {
            if (args.size() < 3) {
                emitOutput("Usage: contact remove <id>", kDimColor);
                break;
            }
            int id = args[2].toInt();
            if (ContactDb::deleteContact(id)) {
                emitOutput("  \xe2\x9c\x93 Contact removed.", kPromptColor);
            } else {
                emitOutput("  Contact not found.", kErrorColor);
            }
            break;
        }
        case TerminalSubcommand::ContactFind: {
            if (args.size() < 3) {
                emitOutput("Usage: contact find <query>", kDimColor);
                break;
            }
            auto results = ContactDb::getAllRecords(args[2]);
            if (results.isEmpty()) {
                emitOutput("  No matches.", kDimColor);
                break;
            }
            for (const auto& c : results) {
                emitOutput("  " + padRight(QString::number(c.id), 4) + padRight(c.name, 20) +
                           truncAddr(c.address));
            }
            break;
        }
        case TerminalSubcommand::ContactLookup: {
            if (args.size() < 3) {
                emitOutput("Usage: contact lookup <address>", kDimColor);
                break;
            }
            QString name = ContactDb::getNameByAddress(args[2]);
            if (name.isEmpty()) {
                emitOutput("  No contact found for this address.", kDimColor);
            } else {
                emitOutput("  " + name, kPromptColor);
            }
            break;
        }
        default:
            break;
    }
}

void TerminalHandler::cmdPortfolio(const TerminalParsedCommand& cmd) {
    const auto& args = cmd.parts;

    switch (cmd.sub) {
        case TerminalSubcommand::None:
        case TerminalSubcommand::PortfolioSummary: {
            auto snap = PortfolioDb::getLatestSnapshotRecord(m_walletAddress);
            if (!snap.has_value()) {
                emitOutput("  No portfolio data.", kDimColor);
                break;
            }
            double totalUsd = snap->totalUsd;
            double solPrice = snap->solPrice;
            emitOutput("  Total Value:  $" + QString::number(totalUsd, 'f', 2) + " USD");
            emitOutput("  SOL Price:    $" + QString::number(solPrice, 'f', 2), kDimColor);
            double costBasis = PortfolioDb::totalCostBasis(m_walletAddress, WSOL_MINT);
            if (costBasis > 0) {
                double pnl = totalUsd - costBasis;
                double pct = (costBasis > 0) ? (pnl / costBasis) * 100.0 : 0;
                QString sign = pnl >= 0 ? "+" : "";
                QColor c = pnl >= 0 ? kPromptColor : kErrorColor;
                emitOutput("  Cost Basis:   $" + QString::number(costBasis, 'f', 2), kDimColor);
                emitOutput("  P&L:          " + sign + "$" + QString::number(qAbs(pnl), 'f', 2) +
                               " (" + sign + QString::number(pct, 'f', 1) + "%)",
                           c);
            }
            break;
        }
        case TerminalSubcommand::PortfolioHistory: {
            int n = (args.size() > 2) ? args[2].toInt() : 7;
            if (n <= 0) {
                n = 7;
            }
            auto snaps = PortfolioDb::getSnapshotsRecords(m_walletAddress, 0,
                                                          std::numeric_limits<qint64>::max());
            if (snaps.isEmpty()) {
                emitOutput("  No snapshots.", kDimColor);
                break;
            }

            emitOutput(padRight("  Date", 22) + "Value (USD)", kDimColor);
            emitOutput("  " + QString(34, QChar(0x2500)), kDimColor);
            QMap<QString, double> dayValues;
            for (const auto& s : snaps) {
                QString day = QDateTime::fromSecsSinceEpoch(s.timestamp).toString("yyyy-MM-dd");
                dayValues[day] = s.totalUsd;
            }
            QStringList days = dayValues.keys();
            int start = qMax(0, days.size() - n);
            for (int i = start; i < days.size(); ++i) {
                emitOutput("  " + padRight(days[i], 20) + "$" +
                           QString::number(dayValues[days[i]], 'f', 2));
            }
            break;
        }
        case TerminalSubcommand::PortfolioLots: {
            if (args.size() < 3) {
                emitOutput("Usage: portfolio lots <mint>", kDimColor);
                break;
            }
            auto lots = PortfolioDb::getOpenLotsRecords(m_walletAddress, args[2]);
            if (lots.isEmpty()) {
                emitOutput("  No open lots.", kDimColor);
                break;
            }
            emitOutput(padRight("  #", 5) + padRight("Acquired", 22) + padRight("Qty", 14) +
                           padRight("Cost/Unit", 12) + "Source",
                       kDimColor);
            emitOutput("  " + QString(60, QChar(0x2500)), kDimColor);
            for (int i = 0; i < lots.size(); ++i) {
                auto& l = lots[i];
                QString date =
                    QDateTime::fromSecsSinceEpoch(l.acquiredAt).toString("yyyy-MM-dd hh:mm");
                emitOutput("  " + padRight(QString::number(i + 1), 4) + padRight(date, 22) +
                           padRight(QString::number(l.remainingQty, 'f', 6), 14) +
                           padRight("$" + QString::number(l.costPerUnit, 'f', 2), 12) + l.source);
            }
            break;
        }
        default:
            break;
    }
}

void TerminalHandler::cmdProgram(const TerminalParsedCommand& cmd) {
    const auto& args = cmd.parts;

    switch (cmd.sub) {
        case TerminalSubcommand::None:
            emitOutput("Usage: program <fetch|is-dex>", kDimColor);
            break;
        case TerminalSubcommand::ProgramFetch: {
            if (args.size() < 3) {
                emitOutput("Usage: program fetch <program_id>", kDimColor);
                break;
            }
            emitOutput("Fetching on-chain IDL...", kDimColor);
            const AnchorIdl::Idl* idl = m_idlRegistry->resolve(args[2]);
            if (idl && idl->isValid()) {
                emitOutput("  \xe2\x9c\x93 " + idl->displayName() + " (" +
                               QString::number(idl->instructions.size()) + " instructions)",
                           kPromptColor);
            } else {
                emitOutput("  Fetch queued. IDL will be cached when available.", kDimColor);
            }
            break;
        }
        case TerminalSubcommand::ProgramIsDex: {
            if (args.size() < 3) {
                emitOutput("Usage: program is-dex <program_id>", kDimColor);
                break;
            }
            if (TxClassifier::isDexProgram(args[2])) {
                emitOutput("  \xe2\x9c\x93 " + TxClassifier::dexName(args[2]), kPromptColor);
            } else {
                emitOutput("  \xe2\x9c\x97 Not a known DEX program.", kDimColor);
            }
            break;
        }
        default:
            break;
    }
}

void TerminalHandler::cmdEncode(const TerminalParsedCommand& cmd) {
    const auto& args = cmd.parts;

    switch (cmd.sub) {
        case TerminalSubcommand::None:
            emitOutput("Usage: encode <base58|decode58|hash>", kDimColor);
            break;
        case TerminalSubcommand::EncodeBase58: {
            if (args.size() < 3) {
                emitOutput("Usage: encode base58 <hex>", kDimColor);
                break;
            }
            QByteArray bytes = QByteArray::fromHex(args[2].toLatin1());
            emitOutput(Base58::encode(bytes));
            break;
        }
        case TerminalSubcommand::EncodeDecode58: {
            if (args.size() < 3) {
                emitOutput("Usage: encode decode58 <base58_string>", kDimColor);
                break;
            }
            QByteArray decoded = Base58::decode(args[2]);
            emitOutput(decoded.toHex());
            break;
        }
        case TerminalSubcommand::EncodeHash: {
            if (args.size() < 3) {
                emitOutput("Usage: encode hash <text>", kDimColor);
                break;
            }
            QString text = args.mid(2).join(' ');
            QByteArray input = text.toUtf8();
            unsigned char hash[crypto_hash_sha256_BYTES];
            crypto_hash_sha256(hash, reinterpret_cast<const unsigned char*>(input.constData()),
                               input.size());
            emitOutput(QByteArray(reinterpret_cast<const char*>(hash), 32).toHex());
            break;
        }
        default:
            break;
    }
}

void TerminalHandler::cmdDb(const TerminalParsedCommand& cmd) {
    switch (cmd.sub) {
        case TerminalSubcommand::None:
            emitOutput("Usage: db stats", kDimColor);
            break;
        case TerminalSubcommand::DbStats: {
            int wallets = WalletDb::countAll();
            int txns = TransactionDb::countTransactions();
            int tokens = TokenAccountDb::countByOwner(m_walletAddress);
            int contacts = ContactDb::countAll();
            int snaps = PortfolioDb::countSnapshots(m_walletAddress);
            int idls = IdlDb::countAll();

            emitOutput("  Wallets:       " + QString::number(wallets));
            emitOutput("  Transactions:  " + QString::number(txns));
            emitOutput("  Token Accts:   " + QString::number(tokens));
            emitOutput("  Contacts:      " + QString::number(contacts));
            emitOutput("  Snapshots:     " + QString::number(snaps));
            emitOutput("  IDL Cache:     " + QString::number(idls) + " programs");
            break;
        }
        default:
            break;
    }
}
