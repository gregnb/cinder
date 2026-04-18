#include "ApprovalExecutor.h"
#include "crypto/Keypair.h"
#include "db/ContactDb.h"
#include "db/Database.h"
#include "db/McpDb.h"
#include "db/NonceDb.h"
#include "db/TokenAccountDb.h"
#include "services/JupiterApi.h"
#include "services/SolanaApi.h"
#include "tx/StakeInstruction.h"
#include "tx/SystemInstruction.h"
#include "tx/TokenInstruction.h"
#include "tx/TokenOperationBuilder.h"
#include "tx/TransactionExecutor.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QtMath>

static constexpr quint64 NONCE_ACCOUNT_SIZE = 80;
static constexpr quint64 STAKE_ACCOUNT_SIZE = 200;

ApprovalExecutor::ApprovalExecutor(QObject* parent) : QObject(parent) {
    m_timer = new QTimer(this);
    m_timer->setInterval(50);
    connect(m_timer, &QTimer::timeout, this, &ApprovalExecutor::poll);

    m_jupiterApi = new JupiterApi(this);
}

void ApprovalExecutor::setSolanaApi(SolanaApi* api) { m_api = api; }
void ApprovalExecutor::setSigner(Signer* signer) { m_signer = signer; }
void ApprovalExecutor::setWalletAddress(const QString& address) { m_walletAddress = address; }
void ApprovalExecutor::setSignerFactory(SignerFactory factory) {
    m_signerFactory = std::move(factory);
}

void ApprovalExecutor::start() { m_timer->start(); }
void ApprovalExecutor::stop() { m_timer->stop(); }

void ApprovalExecutor::poll() {
    if (m_executing) {
        return;
    }

    Database::checkpoint();
    auto records = McpDb::getApprovedUnexecutedRecords();
    if (records.isEmpty()) {
        return;
    }

    // Execute one at a time to avoid concurrent tx signing
    m_executing = true;
    executeApproval(records.first());
}

void ApprovalExecutor::executeApproval(const McpApprovalRecord& record) {
    QJsonDocument doc = QJsonDocument::fromJson(record.arguments.toUtf8());
    QJsonObject args = doc.object();
    const QString& res = record.toolName;

    // Use the approval record's wallet address (from MCP policy binding),
    // falling back to the UI's active wallet if unset
    QString wallet = record.walletAddress.isEmpty() ? m_walletAddress : record.walletAddress;

    if (res == "wallet_send_sol") {
        executeSendSol(record.id, wallet, args);
    } else if (res == "wallet_send_token") {
        executeSendToken(record.id, wallet, args);
    } else if (res == "wallet_add_contact") {
        executeAddContact(record.id, args);
    } else if (res == "wallet_remove_contact") {
        executeRemoveContact(record.id, args);
    } else if (res == "wallet_swap") {
        executeSwap(record.id, wallet, args);
    } else if (res == "wallet_stake_create") {
        executeStakeCreate(record.id, wallet, args);
    } else if (res == "wallet_stake_deactivate") {
        executeStakeDeactivate(record.id, wallet, args);
    } else if (res == "wallet_stake_withdraw") {
        executeStakeWithdraw(record.id, wallet, args);
    } else if (res == "wallet_token_burn") {
        executeTokenBurn(record.id, wallet, args);
    } else if (res == "wallet_token_close") {
        executeTokenClose(record.id, wallet, args);
    } else if (res == "wallet_nonce_create") {
        executeNonceCreate(record.id, wallet, args);
    } else if (res == "wallet_nonce_advance") {
        executeNonceAdvance(record.id, wallet, args);
    } else if (res == "wallet_nonce_withdraw") {
        executeNonceWithdraw(record.id, wallet, args);
    } else if (res == "wallet_nonce_close") {
        executeNonceClose(record.id, wallet, args);
    } else {
        // Unknown tool — mark executed so subprocess doesn't hang
        QJsonObject result;
        result[QLatin1String("status")] = QStringLiteral("completed");
        result[QLatin1String("message")] =
            QStringLiteral("Tool '%1' approved but not recognized").arg(res);
        markCompleted(record.id, result);
    }
}

// ── Signer resolution ──────────────────────────────────────────

Signer* ApprovalExecutor::signerForWallet(const QString& wallet) {
    // If the active signer matches, use it directly
    if (m_signer && m_signer->address() == wallet) {
        return m_signer;
    }

    // Clean up any previous temp signer
    delete m_tempSigner;
    m_tempSigner = nullptr;

    // Ask the factory (CinderWalletApp) to create a signer — it holds the
    // session password so we never store it here.
    if (!m_signerFactory) {
        return nullptr;
    }

    m_tempSigner = m_signerFactory(wallet);
    return m_tempSigner;
}

// ── SOL Transfer ────────────────────────────────────────────────

void ApprovalExecutor::executeSendSol(const QString& id, const QString& wallet,
                                      const QJsonObject& args) {
    if (!m_api || wallet.isEmpty()) {
        markFailed(id, "Wallet not ready (no API or address)");
        return;
    }

    Signer* signer = signerForWallet(wallet);
    if (!signer) {
        markFailed(id, QStringLiteral("Cannot sign for wallet %1").arg(wallet));
        return;
    }

    QString to = args["to"].toString();
    double amount = args["amount"].toDouble();
    quint64 lamports = static_cast<quint64>(amount * 1e9);

    if (to.isEmpty() || lamports == 0) {
        markFailed(id, "Invalid send parameters");
        return;
    }

    SimpleTransactionInput input;
    input.feePayer = wallet;
    input.instructions = {SystemInstruction::transfer(wallet, to, lamports)};
    input.api = m_api;
    input.context = this;
    input.signer = signer;

    SimpleTransactionCallbacks callbacks;
    callbacks.onSent = [this, id](const QString& signature) {
        QJsonObject result;
        result[QLatin1String("status")] = QStringLiteral("completed");
        result[QLatin1String("signature")] = signature;
        markCompleted(id, result);
        emit balancesChanged();
    };
    callbacks.onFailed = [this, id](const QString& error) { markFailed(id, error); };

    executeSimpleTransactionAsync(input, callbacks);
}

// ── SPL Token Transfer ──────────────────────────────────────────

void ApprovalExecutor::executeSendToken(const QString& id, const QString& wallet,
                                        const QJsonObject& args) {
    if (!m_api || wallet.isEmpty()) {
        markFailed(id, "Wallet not ready");
        return;
    }

    Signer* signer = signerForWallet(wallet);
    if (!signer) {
        markFailed(id, QStringLiteral("Cannot sign for wallet %1").arg(wallet));
        return;
    }

    QString to = args["to"].toString();
    QString mint = args["mint"].toString();
    double amount = args["amount"].toDouble();

    if (to.isEmpty() || mint.isEmpty() || amount <= 0) {
        markFailed(id, "Invalid token send parameters");
        return;
    }

    // For SPL token transfers we need the source token account and decimals.
    auto tokenRecord = TokenAccountDb::getTokenRecord(mint);
    if (!tokenRecord.has_value()) {
        markFailed(id, QStringLiteral("Token mint %1 not found in cache").arg(mint));
        return;
    }

    auto accounts = TokenAccountDb::getAccountsByOwnerRecords(wallet);
    QString sourceAta;
    for (const auto& a : accounts) {
        if (a.tokenAddress == mint) {
            sourceAta = a.accountAddress;
            break;
        }
    }
    if (sourceAta.isEmpty()) {
        markFailed(id, QStringLiteral("No token account found for mint %1").arg(mint));
        return;
    }

    TransferInstructionBuildInput buildInput;
    buildInput.walletAddress = wallet;
    buildInput.mint = mint;
    buildInput.sourceTokenAccount = sourceAta;
    buildInput.decimals = tokenRecord->decimals;
    buildInput.tokenProgram = tokenRecord->tokenProgram;
    buildInput.recipients = {{to, amount}};

    auto buildResult = TokenOperationBuilder::buildTransfer(buildInput);
    if (!buildResult.ok) {
        markFailed(id, buildResult.error);
        return;
    }

    SimpleTransactionInput input;
    input.feePayer = wallet;
    input.instructions = buildResult.instructions;
    input.api = m_api;
    input.context = this;
    input.signer = signer;

    SimpleTransactionCallbacks callbacks;
    callbacks.onSent = [this, id](const QString& signature) {
        QJsonObject result;
        result[QLatin1String("status")] = QStringLiteral("completed");
        result[QLatin1String("signature")] = signature;
        markCompleted(id, result);
        emit balancesChanged();
    };
    callbacks.onFailed = [this, id](const QString& error) { markFailed(id, error); };

    executeSimpleTransactionAsync(input, callbacks);
}

// ── Contacts ────────────────────────────────────────────────────

void ApprovalExecutor::executeAddContact(const QString& id, const QJsonObject& args) {
    QString name = args["name"].toString();
    QString address = args["address"].toString();

    if (name.isEmpty() || address.isEmpty()) {
        markFailed(id, "Invalid contact parameters");
        return;
    }

    ContactDb::insertContact(name, address);
    QJsonObject result;
    result[QLatin1String("status")] = QStringLiteral("completed");
    result[QLatin1String("message")] = QStringLiteral("Contact '%1' added").arg(name);
    markCompleted(id, result);
    emit contactsChanged();
}

void ApprovalExecutor::executeRemoveContact(const QString& id, const QJsonObject& args) {
    QString address = args["address"].toString();
    if (address.isEmpty()) {
        markFailed(id, "No address specified");
        return;
    }

    auto contacts = ContactDb::getAllRecords();
    for (const auto& c : contacts) {
        if (c.address == address) {
            ContactDb::deleteContact(c.id);
            QJsonObject result;
            result[QLatin1String("status")] = QStringLiteral("completed");
            result[QLatin1String("message")] = QStringLiteral("Contact '%1' removed").arg(c.name);
            markCompleted(id, result);
            emit contactsChanged();
            return;
        }
    }

    markFailed(id, QStringLiteral("Contact with address %1 not found").arg(address));
}

// ── Stake Create ────────────────────────────────────────────────

void ApprovalExecutor::executeStakeCreate(const QString& id, const QString& wallet,
                                          const QJsonObject& args) {
    if (!m_api || wallet.isEmpty()) {
        markFailed(id, "Wallet not ready");
        return;
    }

    Signer* signer = signerForWallet(wallet);
    if (!signer) {
        markFailed(id, QStringLiteral("Cannot sign for wallet %1").arg(wallet));
        return;
    }

    QString validator = args["validator"].toString();
    double amount = args["amount"].toDouble();
    quint64 stakeLamports = static_cast<quint64>(amount * 1e9);

    if (validator.isEmpty() || stakeLamports == 0) {
        markFailed(id, "Invalid stake parameters");
        return;
    }

    // Generate a new keypair for the stake account
    Keypair stakeKp = Keypair::generate();
    QString stakeAddr = stakeKp.address();

    // Fetch rent-exempt minimum for stake account (200 bytes)
    MinimumBalanceRequest mbReq;
    mbReq.api = m_api;
    mbReq.context = this;
    mbReq.dataSize = STAKE_ACCOUNT_SIZE;
    mbReq.timeoutMs = 10000;

    fetchMinimumBalanceWithTimeout(
        mbReq, {[this, id, wallet, validator, stakeLamports, stakeKp, stakeAddr,
                 signer](quint64 rentLamports) {
                    auto ixs = StakeInstruction::createAndDelegate(wallet, stakeAddr, validator,
                                                                   stakeLamports, rentLamports);

                    SimpleTransactionInput input;
                    input.feePayer = wallet;
                    input.instructions = ixs;
                    input.api = m_api;
                    input.context = this;
                    input.signer = signer;
                    input.appendSignatures =
                        AdditionalSigner::keypairSignatureAppender(stakeKp, "Stake keypair");

                    SimpleTransactionCallbacks callbacks;
                    callbacks.onSent = [this, id, stakeAddr](const QString& signature) {
                        QJsonObject result;
                        result[QLatin1String("status")] = QStringLiteral("completed");
                        result[QLatin1String("signature")] = signature;
                        result[QLatin1String("stake_account")] = stakeAddr;
                        markCompleted(id, result);
                        emit balancesChanged();
                        emit stakeChanged();
                    };
                    callbacks.onFailed = [this, id](const QString& error) {
                        markFailed(id, error);
                    };

                    executeSimpleTransactionAsync(input, callbacks);
                },
                [this, id](const QString& error) {
                    markFailed(id, QStringLiteral("Failed to get rent exemption: %1").arg(error));
                },
                [this, id]() { markFailed(id, "Rent exemption request timed out"); }});
}

// ── Stake Deactivate ────────────────────────────────────────────

void ApprovalExecutor::executeStakeDeactivate(const QString& id, const QString& wallet,
                                              const QJsonObject& args) {
    if (!m_api || wallet.isEmpty()) {
        markFailed(id, "Wallet not ready");
        return;
    }

    Signer* signer = signerForWallet(wallet);
    if (!signer) {
        markFailed(id, QStringLiteral("Cannot sign for wallet %1").arg(wallet));
        return;
    }

    QString stakeAccount = args["stake_account"].toString();
    if (stakeAccount.isEmpty()) {
        markFailed(id, "No stake account specified");
        return;
    }

    SimpleTransactionInput input;
    input.feePayer = wallet;
    input.instructions = {StakeInstruction::deactivate(stakeAccount, wallet)};
    input.api = m_api;
    input.context = this;
    input.signer = signer;

    SimpleTransactionCallbacks callbacks;
    callbacks.onSent = [this, id](const QString& signature) {
        QJsonObject result;
        result[QLatin1String("status")] = QStringLiteral("completed");
        result[QLatin1String("signature")] = signature;
        markCompleted(id, result);
        emit stakeChanged();
    };
    callbacks.onFailed = [this, id](const QString& error) { markFailed(id, error); };

    executeSimpleTransactionAsync(input, callbacks);
}

// ── Stake Withdraw ──────────────────────────────────────────────

void ApprovalExecutor::executeStakeWithdraw(const QString& id, const QString& wallet,
                                            const QJsonObject& args) {
    if (!m_api || wallet.isEmpty()) {
        markFailed(id, "Wallet not ready");
        return;
    }

    Signer* signer = signerForWallet(wallet);
    if (!signer) {
        markFailed(id, QStringLiteral("Cannot sign for wallet %1").arg(wallet));
        return;
    }

    QString stakeAccount = args["stake_account"].toString();
    double amount = args["amount"].toDouble();
    quint64 lamports = static_cast<quint64>(amount * 1e9);

    if (stakeAccount.isEmpty() || lamports == 0) {
        markFailed(id, "Invalid stake withdraw parameters");
        return;
    }

    SimpleTransactionInput input;
    input.feePayer = wallet;
    input.instructions = {StakeInstruction::withdraw(stakeAccount, wallet, wallet, lamports)};
    input.api = m_api;
    input.context = this;
    input.signer = signer;

    SimpleTransactionCallbacks callbacks;
    callbacks.onSent = [this, id](const QString& signature) {
        QJsonObject result;
        result[QLatin1String("status")] = QStringLiteral("completed");
        result[QLatin1String("signature")] = signature;
        markCompleted(id, result);
        emit balancesChanged();
        emit stakeChanged();
    };
    callbacks.onFailed = [this, id](const QString& error) { markFailed(id, error); };

    executeSimpleTransactionAsync(input, callbacks);
}

// ── Token Burn ──────────────────────────────────────────────────

void ApprovalExecutor::executeTokenBurn(const QString& id, const QString& wallet,
                                        const QJsonObject& args) {
    if (!m_api || wallet.isEmpty()) {
        markFailed(id, "Wallet not ready");
        return;
    }

    Signer* signer = signerForWallet(wallet);
    if (!signer) {
        markFailed(id, QStringLiteral("Cannot sign for wallet %1").arg(wallet));
        return;
    }

    QString mint = args["mint"].toString();
    double amount = args["amount"].toDouble();

    if (mint.isEmpty() || amount <= 0) {
        markFailed(id, "Invalid burn parameters");
        return;
    }

    auto tokenRecord = TokenAccountDb::getTokenRecord(mint);
    if (!tokenRecord.has_value()) {
        markFailed(id, QStringLiteral("Token mint %1 not found in cache").arg(mint));
        return;
    }

    // Find source token account
    auto accounts = TokenAccountDb::getAccountsByOwnerRecords(wallet);
    QString sourceAta;
    for (const auto& a : accounts) {
        if (a.tokenAddress == mint) {
            sourceAta = a.accountAddress;
            break;
        }
    }
    if (sourceAta.isEmpty()) {
        markFailed(id, QStringLiteral("No token account found for mint %1").arg(mint));
        return;
    }

    quint64 rawAmount = static_cast<quint64>(amount * qPow(10.0, tokenRecord->decimals));

    SimpleTransactionInput input;
    input.feePayer = wallet;
    input.instructions = {TokenInstruction::burnChecked(
        sourceAta, mint, wallet, rawAmount, tokenRecord->decimals, tokenRecord->tokenProgram)};
    input.api = m_api;
    input.context = this;
    input.signer = signer;

    SimpleTransactionCallbacks callbacks;
    callbacks.onSent = [this, id](const QString& signature) {
        QJsonObject result;
        result[QLatin1String("status")] = QStringLiteral("completed");
        result[QLatin1String("signature")] = signature;
        markCompleted(id, result);
        emit balancesChanged();
    };
    callbacks.onFailed = [this, id](const QString& error) { markFailed(id, error); };

    executeSimpleTransactionAsync(input, callbacks);
}

// ── Token Close ─────────────────────────────────────────────────

void ApprovalExecutor::executeTokenClose(const QString& id, const QString& wallet,
                                         const QJsonObject& args) {
    if (!m_api || wallet.isEmpty()) {
        markFailed(id, "Wallet not ready");
        return;
    }

    Signer* signer = signerForWallet(wallet);
    if (!signer) {
        markFailed(id, QStringLiteral("Cannot sign for wallet %1").arg(wallet));
        return;
    }

    QString tokenAccount = args["token_account"].toString();
    if (tokenAccount.isEmpty()) {
        markFailed(id, "No token account specified");
        return;
    }

    // Determine token program from DB
    QString tokenProgram = SolanaPrograms::TokenProgram;
    auto accounts = TokenAccountDb::getAccountsByOwnerRecords(wallet);
    for (const auto& a : accounts) {
        if (a.accountAddress == tokenAccount) {
            tokenProgram = a.tokenProgram;
            break;
        }
    }

    SimpleTransactionInput input;
    input.feePayer = wallet;
    input.instructions = {
        TokenInstruction::closeAccount(tokenAccount, wallet, wallet, tokenProgram)};
    input.api = m_api;
    input.context = this;
    input.signer = signer;

    SimpleTransactionCallbacks callbacks;
    callbacks.onSent = [this, id](const QString& signature) {
        QJsonObject result;
        result[QLatin1String("status")] = QStringLiteral("completed");
        result[QLatin1String("signature")] = signature;
        markCompleted(id, result);
        emit balancesChanged();
    };
    callbacks.onFailed = [this, id](const QString& error) { markFailed(id, error); };

    executeSimpleTransactionAsync(input, callbacks);
}

// ── Swap (Jupiter) ──────────────────────────────────────────────

void ApprovalExecutor::executeSwap(const QString& id, const QString& wallet,
                                   const QJsonObject& args) {
    if (!m_api || wallet.isEmpty()) {
        markFailed(id, "Wallet not ready");
        return;
    }

    Signer* signer = signerForWallet(wallet);
    if (!signer) {
        markFailed(id, QStringLiteral("Cannot sign for wallet %1").arg(wallet));
        return;
    }

    QString inputMint = args["input_mint"].toString();
    QString outputMint = args["output_mint"].toString();
    double amount = args["amount"].toDouble();
    int slippageBps = args["slippage_bps"].toInt(50);

    if (inputMint.isEmpty() || outputMint.isEmpty() || amount <= 0) {
        markFailed(id, "Invalid swap parameters");
        return;
    }

    // Look up decimals for the input token
    int decimals = 9; // default for SOL/wSOL
    auto tokenRecord = TokenAccountDb::getTokenRecord(inputMint);
    if (tokenRecord.has_value()) {
        decimals = tokenRecord->decimals;
    }

    quint64 rawAmount = static_cast<quint64>(amount * qPow(10.0, decimals));

    // Store pending swap context
    m_pendingSwapId = id;
    m_pendingSwapWallet = wallet;

    // Disconnect any previous connections
    disconnect(m_jupiterApi, nullptr, this, nullptr);

    // Step 1: Fetch quote
    connect(m_jupiterApi, &JupiterApi::quoteReady, this, [this, wallet](const JupiterQuote& quote) {
        disconnect(m_jupiterApi, &JupiterApi::quoteReady, this, nullptr);
        // Step 2: Get swap transaction
        m_jupiterApi->fetchSwapTransaction(quote.rawResponse, wallet);
    });

    connect(m_jupiterApi, &JupiterApi::swapTransactionReady, this,
            [this, signer](const QByteArray& serializedTx, quint64) {
                disconnect(m_jupiterApi, &JupiterApi::swapTransactionReady, this, nullptr);

                if (serializedTx.size() < 66) {
                    markFailed(m_pendingSwapId, "Invalid swap transaction from Jupiter");
                    return;
                }

                // Extract message from serialized tx
                int numSigs = static_cast<unsigned char>(serializedTx[0]);
                int messageOffset = 1 + (numSigs * 64);
                if (messageOffset >= serializedTx.size()) {
                    markFailed(m_pendingSwapId, "Invalid swap transaction structure");
                    return;
                }

                QByteArray message = serializedTx.mid(messageOffset);

                // Step 3: Sign
                signer->signAsync(
                    message, this,
                    [this, serializedTx](const QByteArray& signature, const QString& error) {
                        if (signature.isEmpty()) {
                            markFailed(m_pendingSwapId,
                                       error.isEmpty() ? "Swap signing failed" : error);
                            return;
                        }

                        QByteArray signedTx = serializedTx;
                        signedTx.replace(1, 64, signature);

                        // Step 4: Submit
                        auto sentConn = std::make_shared<QMetaObject::Connection>();
                        auto failConn = std::make_shared<QMetaObject::Connection>();

                        *sentConn = connect(m_api, &SolanaApi::transactionSent, this,
                                            [this, sentConn, failConn](const QString& txSig) {
                                                disconnect(*sentConn);
                                                disconnect(*failConn);
                                                QJsonObject result;
                                                result[QLatin1String("status")] =
                                                    QStringLiteral("completed");
                                                result[QLatin1String("signature")] = txSig;
                                                markCompleted(m_pendingSwapId, result);
                                                emit balancesChanged();
                                            });

                        *failConn = connect(
                            m_api, &SolanaApi::requestFailed, this,
                            [this, sentConn, failConn](const QString& method, const QString& err) {
                                if (method != "sendTransaction") {
                                    return;
                                }
                                disconnect(*sentConn);
                                disconnect(*failConn);
                                markFailed(m_pendingSwapId, err);
                            });

                        m_api->sendTransaction(signedTx, false, "confirmed", 3);
                    });
            });

    connect(m_jupiterApi, &JupiterApi::requestFailed, this,
            [this](const QString&, const QString& error) {
                disconnect(m_jupiterApi, nullptr, this, nullptr);
                markFailed(m_pendingSwapId, QStringLiteral("Jupiter API error: %1").arg(error));
            });

    m_jupiterApi->fetchQuote(inputMint, outputMint, rawAmount, slippageBps);
}

// ── Nonce Create ────────────────────────────────────────────────

void ApprovalExecutor::executeNonceCreate(const QString& id, const QString& wallet,
                                          const QJsonObject& args) {
    if (!m_api || wallet.isEmpty()) {
        markFailed(id, "Wallet not ready");
        return;
    }

    Signer* signer = signerForWallet(wallet);
    if (!signer) {
        markFailed(id, QStringLiteral("Cannot sign for wallet %1").arg(wallet));
        return;
    }

    quint64 lamports = static_cast<quint64>(args["lamports"].toDouble());

    // Generate a new keypair for the nonce account
    Keypair nonceKp = Keypair::generate();
    QString nonceAddr = nonceKp.address();

    if (lamports == 0) {
        // Fetch rent-exempt minimum if not specified
        MinimumBalanceRequest mbReq;
        mbReq.api = m_api;
        mbReq.context = this;
        mbReq.dataSize = NONCE_ACCOUNT_SIZE;
        mbReq.timeoutMs = 10000;

        fetchMinimumBalanceWithTimeout(
            mbReq,
            {[this, id, wallet, nonceKp, nonceAddr, signer](quint64 rentLamports) {
                 auto ixs =
                     SystemInstruction::createNonceAccount(wallet, nonceAddr, wallet, rentLamports);

                 SimpleTransactionInput input;
                 input.feePayer = wallet;
                 input.instructions = ixs;
                 input.api = m_api;
                 input.context = this;
                 input.signer = signer;
                 input.appendSignatures =
                     AdditionalSigner::keypairSignatureAppender(nonceKp, "Nonce keypair");

                 SimpleTransactionCallbacks callbacks;
                 callbacks.onSent = [this, id, nonceAddr, wallet](const QString& signature) {
                     NonceDb::insertNonceAccount(nonceAddr, wallet, QString());
                     QJsonObject result;
                     result[QLatin1String("status")] = QStringLiteral("completed");
                     result[QLatin1String("signature")] = signature;
                     result[QLatin1String("nonce_account")] = nonceAddr;
                     markCompleted(id, result);
                     emit balancesChanged();
                     emit nonceAccountsChanged();
                 };
                 callbacks.onFailed = [this, id](const QString& error) { markFailed(id, error); };

                 executeSimpleTransactionAsync(input, callbacks);
             },
             [this, id](const QString& error) {
                 markFailed(id, QStringLiteral("Failed to get rent exemption: %1").arg(error));
             },
             [this, id]() { markFailed(id, "Rent exemption request timed out"); }});
    } else {
        auto ixs = SystemInstruction::createNonceAccount(wallet, nonceAddr, wallet, lamports);

        SimpleTransactionInput input;
        input.feePayer = wallet;
        input.instructions = ixs;
        input.api = m_api;
        input.context = this;
        input.signer = signer;
        input.appendSignatures =
            AdditionalSigner::keypairSignatureAppender(nonceKp, "Nonce keypair");

        SimpleTransactionCallbacks callbacks;
        callbacks.onSent = [this, id, nonceAddr, wallet](const QString& signature) {
            NonceDb::insertNonceAccount(nonceAddr, wallet, QString());
            QJsonObject result;
            result[QLatin1String("status")] = QStringLiteral("completed");
            result[QLatin1String("signature")] = signature;
            result[QLatin1String("nonce_account")] = nonceAddr;
            markCompleted(id, result);
            emit balancesChanged();
            emit nonceAccountsChanged();
        };
        callbacks.onFailed = [this, id](const QString& error) { markFailed(id, error); };

        executeSimpleTransactionAsync(input, callbacks);
    }
}

// ── Nonce Advance ───────────────────────────────────────────────

void ApprovalExecutor::executeNonceAdvance(const QString& id, const QString& wallet,
                                           const QJsonObject& args) {
    if (!m_api || wallet.isEmpty()) {
        markFailed(id, "Wallet not ready");
        return;
    }

    Signer* signer = signerForWallet(wallet);
    if (!signer) {
        markFailed(id, QStringLiteral("Cannot sign for wallet %1").arg(wallet));
        return;
    }

    QString nonceAccount = args["nonce_account"].toString();
    if (nonceAccount.isEmpty()) {
        markFailed(id, "No nonce account specified");
        return;
    }

    SimpleTransactionInput input;
    input.feePayer = wallet;
    input.instructions = {SystemInstruction::nonceAdvance(nonceAccount, wallet)};
    input.api = m_api;
    input.context = this;
    input.signer = signer;

    SimpleTransactionCallbacks callbacks;
    callbacks.onSent = [this, id](const QString& signature) {
        QJsonObject result;
        result[QLatin1String("status")] = QStringLiteral("completed");
        result[QLatin1String("signature")] = signature;
        markCompleted(id, result);
    };
    callbacks.onFailed = [this, id](const QString& error) { markFailed(id, error); };

    executeSimpleTransactionAsync(input, callbacks);
}

// ── Nonce Withdraw ──────────────────────────────────────────────

void ApprovalExecutor::executeNonceWithdraw(const QString& id, const QString& wallet,
                                            const QJsonObject& args) {
    if (!m_api || wallet.isEmpty()) {
        markFailed(id, "Wallet not ready");
        return;
    }

    Signer* signer = signerForWallet(wallet);
    if (!signer) {
        markFailed(id, QStringLiteral("Cannot sign for wallet %1").arg(wallet));
        return;
    }

    QString nonceAccount = args["nonce_account"].toString();
    double amount = args["amount"].toDouble();
    quint64 lamports = static_cast<quint64>(amount * 1e9);

    if (nonceAccount.isEmpty() || lamports == 0) {
        markFailed(id, "Invalid nonce withdraw parameters");
        return;
    }

    SimpleTransactionInput input;
    input.feePayer = wallet;
    input.instructions = {SystemInstruction::nonceWithdraw(nonceAccount, wallet, wallet, lamports)};
    input.api = m_api;
    input.context = this;
    input.signer = signer;

    SimpleTransactionCallbacks callbacks;
    callbacks.onSent = [this, id](const QString& signature) {
        QJsonObject result;
        result[QLatin1String("status")] = QStringLiteral("completed");
        result[QLatin1String("signature")] = signature;
        markCompleted(id, result);
        emit balancesChanged();
    };
    callbacks.onFailed = [this, id](const QString& error) { markFailed(id, error); };

    executeSimpleTransactionAsync(input, callbacks);
}

// ── Nonce Close ─────────────────────────────────────────────────

void ApprovalExecutor::executeNonceClose(const QString& id, const QString& wallet,
                                         const QJsonObject& args) {
    if (!m_api || wallet.isEmpty()) {
        markFailed(id, "Wallet not ready");
        return;
    }

    Signer* signer = signerForWallet(wallet);
    if (!signer) {
        markFailed(id, QStringLiteral("Cannot sign for wallet %1").arg(wallet));
        return;
    }

    QString nonceAccount = args["nonce_account"].toString();
    if (nonceAccount.isEmpty()) {
        markFailed(id, "No nonce account specified");
        return;
    }

    // Fetch the nonce account's balance to withdraw everything
    auto acctConn = std::make_shared<QMetaObject::Connection>();
    auto failConn = std::make_shared<QMetaObject::Connection>();

    *acctConn = connect(
        m_api, &SolanaApi::accountInfoReady, this,
        [this, id, wallet, nonceAccount, signer, acctConn,
         failConn](const QString& address, const QByteArray&, const QString&, quint64 lamports) {
            if (address != nonceAccount) {
                return;
            }
            disconnect(*acctConn);
            disconnect(*failConn);

            if (lamports == 0) {
                markFailed(id, "Nonce account has zero balance");
                return;
            }

            SimpleTransactionInput input;
            input.feePayer = wallet;
            input.instructions = {
                SystemInstruction::nonceWithdraw(nonceAccount, wallet, wallet, lamports)};
            input.api = m_api;
            input.context = this;
            input.signer = signer;

            SimpleTransactionCallbacks callbacks;
            callbacks.onSent = [this, id, nonceAccount](const QString& signature) {
                NonceDb::deleteNonceAccount(nonceAccount);
                QJsonObject result;
                result[QLatin1String("status")] = QStringLiteral("completed");
                result[QLatin1String("signature")] = signature;
                markCompleted(id, result);
                emit balancesChanged();
                emit nonceAccountsChanged();
            };
            callbacks.onFailed = [this, id](const QString& error) { markFailed(id, error); };

            executeSimpleTransactionAsync(input, callbacks);
        });

    *failConn =
        connect(m_api, &SolanaApi::requestFailed, this,
                [this, id, failConn, acctConn](const QString& method, const QString& error) {
                    if (method != "getAccountInfo") {
                        return;
                    }
                    disconnect(*failConn);
                    disconnect(*acctConn);
                    markFailed(id, QStringLiteral("Failed to fetch nonce account: %1").arg(error));
                });

    m_api->fetchAccountInfo(nonceAccount);
}

// ── Result helpers ──────────────────────────────────────────────

void ApprovalExecutor::markCompleted(const QString& id, const QJsonObject& result) {
    McpDb::markExecuted(id,
                        QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact)));
    m_executing = false;
}

void ApprovalExecutor::markFailed(const QString& id, const QString& error) {
    QJsonObject result;
    result[QLatin1String("status")] = QStringLiteral("failed");
    result[QLatin1String("error")] = error;
    McpDb::markExecuted(id,
                        QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact)));
    m_executing = false;
}
