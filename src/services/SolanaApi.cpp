#include "SolanaApi.h"
#include "tx/ProgramIds.h"
#include <QDateTime>
#include <QDebug>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <memory>

static const QString DEFAULT_RPC_URL = "https://api.mainnet-beta.solana.com";

SolanaApi::SolanaApi(QObject* parent) : QObject(parent), m_rpcUrls({DEFAULT_RPC_URL}) {
    m_drainTimer.setSingleShot(true);
    connect(&m_drainTimer, &QTimer::timeout, this, &SolanaApi::processQueue);

    m_statsTimer.setInterval(60000);
    connect(&m_statsTimer, &QTimer::timeout, this, &SolanaApi::logRpcStats);
    m_statsTimer.start();
}

SolanaApi::SolanaApi(const QString& rpcUrl, QObject* parent)
    : QObject(parent), m_rpcUrls({rpcUrl}) {
    m_drainTimer.setSingleShot(true);
    connect(&m_drainTimer, &QTimer::timeout, this, &SolanaApi::processQueue);

    m_statsTimer.setInterval(60000);
    connect(&m_statsTimer, &QTimer::timeout, this, &SolanaApi::logRpcStats);
    m_statsTimer.start();
}

void SolanaApi::setRpcInterceptorForTests(
    const std::function<std::optional<QJsonObject>(const QString&, const QJsonArray&)>&
        interceptor) {
    m_rpcInterceptor = interceptor;
}

void SolanaApi::setMethodLimitForTests(const QString& method, int limit) {
    updateMethodLimit(method, limit);
}

// ── Multi-endpoint pool ──────────────────────────────────────

void SolanaApi::setRpcUrls(const QStringList& urls) {
    m_rpcUrls = urls;
    if (m_rpcUrls.isEmpty()) {
        m_rpcUrls.append(DEFAULT_RPC_URL);
    }
    m_nextUrlIndex = 0;
}

QStringList SolanaApi::rpcUrls() const { return m_rpcUrls; }

void SolanaApi::addRpcUrl(const QString& url) {
    const QString trimmed = url.trimmed();
    if (!trimmed.isEmpty() && !m_rpcUrls.contains(trimmed)) {
        m_rpcUrls.append(trimmed);
    }
}

void SolanaApi::removeRpcUrl(const QString& url) {
    if (url == DEFAULT_RPC_URL) {
        return;
    }
    m_rpcUrls.removeAll(url);
    if (m_rpcUrls.isEmpty()) {
        m_rpcUrls.append(DEFAULT_RPC_URL);
    }
    if (m_nextUrlIndex >= m_rpcUrls.size()) {
        m_nextUrlIndex = 0;
    }
}

void SolanaApi::setRpcUrl(const QString& url) {
    m_rpcUrls = {url.trimmed().isEmpty() ? DEFAULT_RPC_URL : url.trimmed()};
    m_nextUrlIndex = 0;
}

QString SolanaApi::rpcUrl() const {
    return m_rpcUrls.isEmpty() ? DEFAULT_RPC_URL : m_rpcUrls.first();
}

QString SolanaApi::nextRpcUrl() {
    if (m_rpcUrls.isEmpty()) {
        return DEFAULT_RPC_URL;
    }
    const QString& url = m_rpcUrls[m_nextUrlIndex];
    m_nextUrlIndex = (m_nextUrlIndex + 1) % m_rpcUrls.size();
    return url;
}

// ── Priority queue ────────────────────────────────────────────

void SolanaApi::enqueueRpc(const QString& method, const QJsonArray& params,
                           const std::function<void(const QJsonObject&)>& onSuccess,
                           Priority priority) {
    PendingRpc req{method, params, onSuccess, priority};

    // Drop non-high-priority requests when the queue is too deep (e.g. app was
    // backgrounded overnight and timers kept firing while network was stalled).
    constexpr int kMaxNormalQueueDepth = 50;
    if (priority == Priority::High) {
        m_highQueue.enqueue(std::move(req));
    } else if (priority == Priority::Low) {
        if (m_lowQueue.size() >= kMaxNormalQueueDepth) {
            return;
        }
        m_lowQueue.enqueue(std::move(req));
    } else {
        if (m_normalQueue.size() >= kMaxNormalQueueDepth) {
            return;
        }
        m_normalQueue.enqueue(std::move(req));
    }

    const char* tag = priority == Priority::High  ? "(HIGH)"
                      : priority == Priority::Low ? "(LOW)"
                                                  : "(NORMAL)";
    qDebug() << "[RpcQueue] Enqueue" << method << tag << "| queues: high=" << m_highQueue.size()
             << "normal=" << m_normalQueue.size() << "low=" << m_lowQueue.size()
             << "active=" << m_activeRequests << "tokens=" << availableTokens();

    processQueue();
}

int SolanaApi::availableTokens() {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 windowStart = now - RATE_WINDOW_MS;

    // Prune timestamps older than the rolling window
    while (!m_requestTimestamps.isEmpty() && m_requestTimestamps.first() < windowStart) {
        m_requestTimestamps.removeFirst();
    }

    return RATE_LIMIT - m_requestTimestamps.size();
}

qint64 SolanaApi::dataUsedInWindow() {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 windowStart = now - DATA_WINDOW_MS;

    // Prune old samples
    while (!m_dataSamples.isEmpty() && m_dataSamples.first().timestamp < windowStart) {
        m_dataSamples.removeFirst();
    }

    qint64 total = 0;
    for (const auto& s : m_dataSamples) {
        total += s.bytes;
    }
    return total;
}

int SolanaApi::availableMethodTokens(const QString& method) {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 windowStart = now - METHOD_RATE_WINDOW_MS; // 10s window matching server

    auto& timestamps = m_methodTimestamps[method];
    while (!timestamps.isEmpty() && timestamps.first() < windowStart) {
        timestamps.removeFirst();
    }

    int limit = m_methodLimits.value(method, DEFAULT_METHOD_LIMIT);
    return limit - timestamps.size();
}

bool SolanaApi::isMethodInBackoff(const QString& method) {
    auto it = m_methodBackoffUntil.find(method);
    if (it == m_methodBackoffUntil.end()) {
        return false;
    }
    if (QDateTime::currentMSecsSinceEpoch() >= it.value()) {
        m_methodBackoffUntil.erase(it);
        return false;
    }
    return true;
}

bool SolanaApi::dequeueFirstDispatchable(QQueue<PendingRpc>& queue, PendingRpc& out) {
    for (int i = 0; i < queue.size(); i++) {
        if (isMethodInBackoff(queue[i].method)) {
            continue;
        }
        if (availableMethodTokens(queue[i].method) > 0) {
            out = queue[i];
            queue.removeAt(i);
            return true;
        }
    }
    return false;
}

void SolanaApi::reEnqueue(PendingRpc req) {
    if (req.priority == Priority::High) {
        m_highQueue.enqueue(std::move(req));
    } else if (req.priority == Priority::Low) {
        m_lowQueue.enqueue(std::move(req));
    } else {
        m_normalQueue.enqueue(std::move(req));
    }
}

void SolanaApi::updateMethodLimit(const QString& method, int limit) {
    if (limit <= 0) {
        return;
    }
    int current = m_methodLimits.value(method, -1);
    if (current == limit) {
        return; // already known
    }
    if (current == -1) {
        // First time learning this method's limit
        qDebug() << "[RpcQueue] Learned method limit:" << method << "=" << limit << "req/10s";
        m_methodLimits[method] = limit;
    } else if (limit < current) {
        // Server lowered the limit — respect it
        qDebug() << "[RpcQueue] Method limit lowered:" << method << current << "->" << limit
                 << "req/10s";
        m_methodLimits[method] = limit;
    }
    // If limit > current, keep our more conservative value
}

void SolanaApi::processQueue() {
    // Dispatch as many requests as tokens and concurrency allow
    while (!m_highQueue.isEmpty() || !m_normalQueue.isEmpty() || !m_lowQueue.isEmpty()) {
        if (m_activeRequests >= MAX_CONCURRENT) {
            break;
        }
        if (availableTokens() <= 0) {
            break;
        }

        // Find first dispatchable request: high → normal → low
        // Skips requests whose method is at its per-method rate limit,
        // so a blocked getTransaction doesn't hold up a getBalance behind it.
        PendingRpc req;
        const char* tag = nullptr;
        if (dequeueFirstDispatchable(m_highQueue, req)) {
            tag = "(HIGH)";
        } else if (dequeueFirstDispatchable(m_normalQueue, req)) {
            tag = "(NORMAL)";
        } else if (dequeueFirstDispatchable(m_lowQueue, req)) {
            tag = "(LOW)";
        } else {
            break; // All queued items are method-limited
        }

        qDebug() << "[RpcQueue] Dispatch" << req.method << tag << "| tokens:" << availableTokens()
                 << "method(" << req.method << "):" << availableMethodTokens(req.method)
                 << "| active:" << m_activeRequests << "queued: high=" << m_highQueue.size()
                 << "normal=" << m_normalQueue.size() << "low=" << m_lowQueue.size();

        sendRpc(req);
    }

    // If work remains, schedule drain for when next token or slot frees up
    if (!m_highQueue.isEmpty() || !m_normalQueue.isEmpty() || !m_lowQueue.isEmpty()) {
        scheduleDrain();
    }
}

void SolanaApi::scheduleDrain() {
    if (m_drainTimer.isActive()) {
        return;
    }

    // If at concurrent limit, the completion handler will trigger processQueue
    if (m_activeRequests >= MAX_CONCURRENT) {
        return;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    qint64 nextExpiry = 0;

    // Global token expiry
    if (availableTokens() <= 0 && !m_requestTimestamps.isEmpty()) {
        nextExpiry = m_requestTimestamps.first() + RATE_WINDOW_MS;
    }

    // Per-method token expiry and backoff expiry — find the soonest unblock
    auto checkMethodExpiry = [&](const QQueue<PendingRpc>& queue) {
        for (const auto& req : queue) {
            // Check per-method backoff
            auto backoffIt = m_methodBackoffUntil.find(req.method);
            if (backoffIt != m_methodBackoffUntil.end() && backoffIt.value() > now) {
                if (nextExpiry == 0 || backoffIt.value() < nextExpiry) {
                    nextExpiry = backoffIt.value();
                }
                continue;
            }
            // Check per-method rate limit
            if (availableMethodTokens(req.method) <= 0) {
                auto& ts = m_methodTimestamps[req.method];
                if (!ts.isEmpty()) {
                    qint64 expiry = ts.first() + METHOD_RATE_WINDOW_MS;
                    if (nextExpiry == 0 || expiry < nextExpiry) {
                        nextExpiry = expiry;
                    }
                }
            }
        }
    };
    checkMethodExpiry(m_highQueue);
    checkMethodExpiry(m_normalQueue);
    checkMethodExpiry(m_lowQueue);

    if (nextExpiry == 0) {
        m_drainTimer.start(0);
        return;
    }

    int delayMs = qMax(1, static_cast<int>(nextExpiry - now));

    qDebug() << "[RpcQueue] Rate limited — next token in" << delayMs << "ms"
             << "| active:" << m_activeRequests << "queued: high=" << m_highQueue.size()
             << "normal=" << m_normalQueue.size() << "low=" << m_lowQueue.size();

    m_drainTimer.start(delayMs);
}

void SolanaApi::sendRpc(const PendingRpc& req) {
    m_activeRequests++;
    m_totalRequests++;
    m_requestCounts[req.method]++;
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    m_requestTimestamps.append(now);
    m_methodTimestamps[req.method].append(now);

    QJsonObject body;
    body["jsonrpc"] = "2.0";
    body["id"] = m_nextId++;
    body["method"] = req.method;
    body["params"] = req.params;

    if (m_rpcInterceptor) {
        const auto mocked = m_rpcInterceptor(req.method, req.params);
        QTimer::singleShot(0, this, [this, pendingReq = req, mocked]() mutable {
            m_activeRequests--;
            const QString& method = pendingReq.method;

            if (!mocked.has_value()) {
                emit requestFailed(method, "Mock RPC response missing");
                processQueue();
                return;
            }

            const QJsonObject& root = mocked.value();
            if (root.contains("error")) {
                const QJsonObject err = root["error"].toObject();
                emit requestFailed(method, QString("RPC error %1: %2")
                                               .arg(err["code"].toInt())
                                               .arg(err["message"].toString()));
                processQueue();
                return;
            }

            pendingReq.onSuccess(root);
            processQueue();
        });
        return;
    }

    QNetworkRequest request(nextRpcUrl());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

    QNetworkReply* reply = m_nam.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));

    connect(reply, &QNetworkReply::finished, this, [this, reply, pendingReq = req]() mutable {
        reply->deleteLater();
        m_activeRequests--;

        const QString& method = pendingReq.method;
        const QByteArray data = reply->readAll();
        qint64 responseBytes = data.size();

        // Track data volume in rolling window
        m_dataSamples.append({QDateTime::currentMSecsSinceEpoch(), responseBytes});
        qint64 windowBytes = dataUsedInWindow();

        // Log rate-limit headers on every response
        QByteArray methodLimitHeader = reply->rawHeader("x-ratelimit-method-limit");
        QByteArray methodRemaining = reply->rawHeader("x-ratelimit-method-remaining");
        QByteArray globalLimit = reply->rawHeader("x-ratelimit-limit");
        QByteArray globalRemaining = reply->rawHeader("x-ratelimit-remaining");

        if (!methodLimitHeader.isEmpty()) {
            updateMethodLimit(method, QString::fromUtf8(methodLimitHeader).toInt());
        }

        // Log headers when they carry useful info
        if (!methodLimitHeader.isEmpty() || !globalLimit.isEmpty()) {
            qDebug() << "[RpcHeaders]" << method
                     << "| method-limit:" << (methodLimitHeader.isEmpty() ? "-" : methodLimitHeader)
                     << "method-remaining:" << (methodRemaining.isEmpty() ? "-" : methodRemaining)
                     << "| global-limit:" << (globalLimit.isEmpty() ? "-" : globalLimit)
                     << "global-remaining:" << (globalRemaining.isEmpty() ? "-" : globalRemaining);
        }

        if (reply->error() != QNetworkReply::NoError) {
            int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

            if (httpStatus == 429) {
                // ── 429: Re-enqueue + per-method backoff (invisible to consumers) ──
                m_total429s++;
                m_429Counts[method]++;

                // Log ALL headers on 429 for debugging
                const auto headerPairs = reply->rawHeaderPairs();
                for (const auto& pair : headerPairs) {
                    QString name = QString::fromUtf8(pair.first).toLower();
                    if (name.contains("rate") || name.contains("limit") || name.contains("retry") ||
                        name.contains("throttl") || name.contains("reset") ||
                        name.contains("remain") || name.contains("backoff") ||
                        name.contains("cooldown")) {
                        qDebug() << "[RpcQueue] 429 header:" << pair.first << "=" << pair.second;
                    }
                }

                // Compute per-method backoff from Retry-After header
                QString retryAfter = QString::fromUtf8(reply->rawHeader("Retry-After"));
                int backoffMs = retryAfter.isEmpty() ? BACKOFF_BASE_MS : retryAfter.toInt() * 1000;
                if (backoffMs <= 0) {
                    backoffMs = BACKOFF_BASE_MS;
                }
                backoffMs = qMin(backoffMs, BACKOFF_CAP_MS);

                qint64 backoffUntil = QDateTime::currentMSecsSinceEpoch() + backoffMs;
                m_methodBackoffUntil[method] = backoffUntil;

                qDebug() << "[RpcQueue] 429 on" << method
                         << "| Retry-After:" << (retryAfter.isEmpty() ? "(none)" : retryAfter)
                         << "| method backoff:" << backoffMs << "ms"
                         << "| re-enqueuing (retries:" << pendingReq.retries << ")"
                         << "| queued: high=" << m_highQueue.size()
                         << "normal=" << m_normalQueue.size() << "low=" << m_lowQueue.size();

                // Re-enqueue — does NOT emit requestFailed
                reEnqueue(std::move(pendingReq));
                scheduleDrain();
                return;
            }

            // ── Non-429 network error: retry up to MAX_RETRIES ──
            pendingReq.retries++;
            if (pendingReq.retries <= PendingRpc::MAX_RETRIES) {
                qDebug() << "[RpcQueue] Network error on" << method << ":" << reply->errorString()
                         << "| retry" << pendingReq.retries << "/" << PendingRpc::MAX_RETRIES;
                reEnqueue(std::move(pendingReq));
                processQueue();
                return;
            }

            qDebug() << "[RpcQueue] Failed (max retries) on" << method << ":"
                     << reply->errorString() << "| bytes:" << responseBytes
                     << "data(30s):" << (windowBytes / 1024) << "KB";
            emit requestFailed(method,
                               QString("HTTP %1: %2").arg(httpStatus).arg(reply->errorString()));
            processQueue();
            return;
        }

        // ── Successful HTTP response ──

        qDebug() << "[RpcQueue] Complete" << method << "| bytes:" << responseBytes
                 << "data(30s):" << (windowBytes / 1024) << "KB"
                 << "| active:" << m_activeRequests << "tokens:" << availableTokens() << "method("
                 << method << "):" << availableMethodTokens(method)
                 << "queued: high=" << m_highQueue.size() << "normal=" << m_normalQueue.size()
                 << "low=" << m_lowQueue.size();

        // Clear method backoff on success
        m_methodBackoffUntil.remove(method);

        QJsonParseError parseErr;
        const QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);

        if (parseErr.error != QJsonParseError::NoError) {
            emit requestFailed(method, "JSON parse error: " + parseErr.errorString());
            processQueue();
            return;
        }

        const QJsonObject root = doc.object();

        // Check for JSON-RPC error (permanent — don't retry)
        if (root.contains("error")) {
            const QJsonObject err = root["error"].toObject();
            emit requestFailed(method, QString("RPC error %1: %2")
                                           .arg(err["code"].toInt())
                                           .arg(err["message"].toString()));
            processQueue();
            return;
        }

        pendingReq.onSuccess(root);
        processQueue();
    });
}

// ── JSON-RPC transport (backward compatible — normal priority) ─

void SolanaApi::postRpc(const QString& method, const QJsonArray& params,
                        const std::function<void(const QJsonObject&)>& onSuccess) {
    enqueueRpc(method, params, onSuccess, Priority::Normal);
}

void SolanaApi::postRpcHighPriority(const QString& method, const QJsonArray& params,
                                    const std::function<void(const QJsonObject&)>& onSuccess) {
    enqueueRpc(method, params, onSuccess, Priority::High);
}

void SolanaApi::postRpcLowPriority(const QString& method, const QJsonArray& params,
                                   const std::function<void(const QJsonObject&)>& onSuccess) {
    enqueueRpc(method, params, onSuccess, Priority::Low);
}

// ── Read methods ──────────────────────────────────────────

void SolanaApi::fetchBalance(const QString& address) {
    QJsonArray params;
    params.append(address);

    postRpc("getBalance", params, [this, address](const QJsonObject& root) {
        quint64 lamports = static_cast<quint64>(root["result"].toObject()["value"].toInteger());
        emit balanceReady(address, lamports);
    });
}

void SolanaApi::fetchTokenAccountsByOwner(const QString& ownerAddress) {
    QJsonObject encoding;
    encoding["encoding"] = "jsonParsed";

    // Shared state to merge results from both Token Program and Token-2022.
    // Each call (success or failure) decrements remaining; emit once both done.
    auto merged = std::make_shared<QJsonArray>();
    auto remaining = std::make_shared<int>(2);
    auto guard = new QObject(this);

    auto emitIfDone = [this, ownerAddress, merged, remaining, guard]() {
        if (--(*remaining) == 0) {
            emit tokenAccountsReady(ownerAddress, *merged);
            guard->deleteLater();
        }
    };

    // On failure for getTokenAccountsByOwner, still count down so the cycle completes.
    connect(this, &SolanaApi::requestFailed, guard,
            [emitIfDone](const QString& method, const QString&) {
                if (method == "getTokenAccountsByOwner") {
                    emitIfDone();
                }
            });

    // Legacy Token Program
    QJsonObject legacyFilter;
    legacyFilter["programId"] = SolanaPrograms::TokenProgram;
    QJsonArray legacyParams;
    legacyParams.append(ownerAddress);
    legacyParams.append(legacyFilter);
    legacyParams.append(encoding);

    postRpc("getTokenAccountsByOwner", legacyParams, [merged, emitIfDone](const QJsonObject& root) {
        const QJsonArray accounts = root["result"].toObject()["value"].toArray();
        for (const auto& a : accounts) {
            merged->append(a);
        }
        emitIfDone();
    });

    // Token-2022 Program
    QJsonObject t22Filter;
    t22Filter["programId"] = SolanaPrograms::Token2022Program;
    QJsonArray t22Params;
    t22Params.append(ownerAddress);
    t22Params.append(t22Filter);
    t22Params.append(encoding);

    postRpc("getTokenAccountsByOwner", t22Params, [merged, emitIfDone](const QJsonObject& root) {
        const QJsonArray accounts = root["result"].toObject()["value"].toArray();
        for (const auto& a : accounts) {
            merged->append(a);
        }
        emitIfDone();
    });
}

void SolanaApi::fetchSignatures(const QString& address, int limit, const QString& before) {
    QJsonObject options;
    options["limit"] = limit;
    if (!before.isEmpty()) {
        options["before"] = before;
    }

    QJsonArray params;
    params.append(address);
    params.append(options);

    postRpc("getSignaturesForAddress", params, [this, address](const QJsonObject& root) {
        emit signaturesReady(address, SignatureInfo::fromJsonArray(root["result"].toArray()));
    });
}

void SolanaApi::fetchTransaction(const QString& signature) {
    QJsonObject options;
    options["encoding"] = "jsonParsed";
    options["maxSupportedTransactionVersion"] = 0;

    QJsonArray params;
    params.append(signature);
    params.append(options);

    postRpc("getTransaction", params, [this, signature](const QJsonObject& root) {
        emit transactionReady(signature, TransactionResponse::fromJson(root["result"].toObject()));
    });
}

void SolanaApi::fetchRecentPrioritizationFees(const QStringList& accounts) {
    QJsonArray params;
    if (!accounts.isEmpty()) {
        QJsonArray addrs;
        for (const auto& a : accounts) {
            addrs.append(a);
        }
        params.append(addrs);
    }

    postRpcHighPriority("getRecentPrioritizationFees", params, [this](const QJsonObject& root) {
        emit prioritizationFeesReady(PriorityFee::fromJsonArray(root["result"].toArray()));
    });
}

void SolanaApi::fetchMinimumBalanceForRentExemption(quint64 dataSize) {
    QJsonArray params;
    params.append(static_cast<qint64>(dataSize));

    postRpcHighPriority(
        "getMinimumBalanceForRentExemption", params, [this](const QJsonObject& root) {
            emit minimumBalanceReady(static_cast<quint64>(root["result"].toInteger()));
        });
}

void SolanaApi::fetchNonceAccount(const QString& nonceAddress, const QString& commitment) {
    QJsonObject options;
    options["encoding"] = "base64";
    options["commitment"] = commitment;

    QJsonArray params;
    params.append(nonceAddress);
    params.append(options);

    postRpcHighPriority("getAccountInfo", params, [this, nonceAddress](const QJsonObject& root) {
        const QJsonObject value = root["result"].toObject()["value"].toObject();
        const QJsonArray dataArr = value["data"].toArray();
        const QByteArray raw = QByteArray::fromBase64(dataArr[0].toString().toUtf8());
        emit nonceAccountReady(nonceAddress, NonceAccount::fromAccountData(raw));
    });
}

void SolanaApi::fetchAccountInfo(const QString& address, const QString& encoding) {
    QJsonObject options;
    options["encoding"] = encoding;

    QJsonArray params;
    params.append(address);
    params.append(options);

    postRpcHighPriority("getAccountInfo", params, [this, address](const QJsonObject& root) {
        const QJsonObject value = root["result"].toObject()["value"].toObject();
        if (value.isEmpty()) {
            emit accountInfoReady(address, QByteArray(), QString(), 0);
            return;
        }
        const QJsonArray dataArr = value["data"].toArray();
        QByteArray data = QByteArray::fromBase64(dataArr[0].toString().toUtf8());
        QString owner = value["owner"].toString();
        quint64 lamports = static_cast<quint64>(value["lamports"].toDouble());
        emit accountInfoReady(address, data, owner, lamports);
    });
}

void SolanaApi::fetchLatestBlockhash(const QString& commitment) {
    QJsonObject options;
    options["commitment"] = commitment;

    QJsonArray params;
    params.append(options);

    postRpcHighPriority("getLatestBlockhash", params, [this](const QJsonObject& root) {
        const QJsonObject value = root["result"].toObject()["value"].toObject();
        emit latestBlockhashReady(value["blockhash"].toString(),
                                  static_cast<quint64>(value["lastValidBlockHeight"].toInteger()));
    });
}

// ── Staking methods ───────────────────────────────────────

void SolanaApi::fetchVoteAccounts() {
    postRpc("getVoteAccounts", QJsonArray{}, [this](const QJsonObject& root) {
        QJsonObject result = root["result"].toObject();
        emit voteAccountsReady(result["current"].toArray(), result["delinquent"].toArray());
    });
}

void SolanaApi::fetchInflationRate() {
    postRpc("getInflationRate", QJsonArray{}, [this](const QJsonObject& root) {
        QJsonObject r = root["result"].toObject();
        emit inflationRateReady(r["total"].toDouble(), r["validator"].toDouble(),
                                r["foundation"].toDouble(), r["epoch"].toDouble());
    });
}

void SolanaApi::fetchEpochInfo(const QString& commitment) {
    QJsonObject opts;
    opts["commitment"] = commitment;
    QJsonArray params;
    params.append(opts);

    postRpc("getEpochInfo", params, [this](const QJsonObject& root) {
        QJsonObject r = root["result"].toObject();
        emit epochInfoReady(static_cast<quint64>(r["epoch"].toDouble()),
                            static_cast<quint64>(r["slotIndex"].toDouble()),
                            static_cast<quint64>(r["slotsInEpoch"].toDouble()),
                            static_cast<quint64>(r["absoluteSlot"].toDouble()));
    });
}

void SolanaApi::fetchStakeMinimumDelegation(const QString& commitment) {
    QJsonObject opts;
    opts["commitment"] = commitment;
    QJsonArray params;
    params.append(opts);

    postRpc("getStakeMinimumDelegation", params, [this](const QJsonObject& root) {
        emit stakeMinimumDelegationReady(
            static_cast<quint64>(root["result"].toObject()["value"].toInteger()));
    });
}

void SolanaApi::fetchStakeAccounts(const QString& walletAddress) {
    // Filter by withdrawer pubkey at offset 44 in stake account data
    QJsonObject memcmp;
    memcmp["offset"] = 44;
    memcmp["bytes"] = walletAddress;

    QJsonObject filterObj;
    filterObj["memcmp"] = memcmp;

    QJsonArray filters;
    filters.append(filterObj);

    QJsonObject opts;
    opts["encoding"] = "jsonParsed";
    opts["filters"] = filters;

    QJsonArray params;
    params.append(SolanaPrograms::StakeProgram);
    params.append(opts);

    postRpc("getProgramAccounts", params, [this, walletAddress](const QJsonObject& root) {
        QJsonArray arr = root["result"].toArray();
        qWarning() << "[SolanaApi] getProgramAccounts (stake) returned" << arr.size()
                   << "accounts for" << walletAddress;
        emit stakeAccountsReady(walletAddress, arr);
    });
}

void SolanaApi::fetchInflationReward(const QString& address, quint64 epoch) {
    QJsonArray addresses;
    addresses.append(address);

    QJsonObject opts;
    opts["epoch"] = static_cast<qint64>(epoch);

    QJsonArray params;
    params.append(addresses);
    params.append(opts);

    postRpc("getInflationReward", params, [this, address, epoch](const QJsonObject& root) {
        const QJsonArray result = root["result"].toArray();
        if (result.isEmpty() || result.at(0).isNull()) {
            emit inflationRewardReady(address, epoch, false, 0, 0, 0, -1);
            return;
        }

        const QJsonObject reward = result.at(0).toObject();
        emit inflationRewardReady(address, epoch, true, reward["amount"].toInteger(),
                                  static_cast<quint64>(reward["postBalance"].toInteger()),
                                  static_cast<quint64>(reward["effectiveSlot"].toInteger()),
                                  reward.contains("commission") && !reward["commission"].isNull()
                                      ? reward["commission"].toInt(-1)
                                      : -1);
    });
}

void SolanaApi::fetchSupply(const QString& commitment) {
    QJsonObject opts;
    opts["commitment"] = commitment;
    opts["excludeNonCirculatingAccountsList"] = true;
    QJsonArray params;
    params.append(opts);

    postRpc("getSupply", params, [this](const QJsonObject& root) {
        QJsonObject v = root["result"].toObject()["value"].toObject();
        emit supplyReady(static_cast<quint64>(v["total"].toDouble()),
                         static_cast<quint64>(v["circulating"].toDouble()));
    });
}

// ── Network stats methods ─────────────────────────────────

void SolanaApi::fetchRecentPerformanceSamples(int limit) {
    QJsonArray params;
    params.append(limit);
    postRpc("getRecentPerformanceSamples", params, [this](const QJsonObject& root) {
        emit performanceSamplesReady(root["result"].toArray());
    });
}

void SolanaApi::fetchBlockHeight(const QString& commitment) {
    QJsonObject opts;
    opts["commitment"] = commitment;
    QJsonArray params;
    params.append(opts);
    postRpc("getBlockHeight", params, [this](const QJsonObject& root) {
        emit blockHeightReady(static_cast<quint64>(root["result"].toDouble()));
    });
}

void SolanaApi::fetchVersion() {
    postRpc("getVersion", QJsonArray{}, [this](const QJsonObject& root) {
        emit versionReady(root["result"].toObject()["solana-core"].toString());
    });
}

void SolanaApi::fetchSignatureStatuses(const QStringList& signatures) {
    QJsonArray sigArray;
    for (const auto& sig : signatures) {
        sigArray.append(sig);
    }
    QJsonObject options;
    options["searchTransactionHistory"] = true;
    postRpcHighPriority(
        "getSignatureStatuses", QJsonArray{sigArray, options}, [this](const QJsonObject& root) {
            emit signatureStatusesReady(root["result"].toObject()["value"].toArray());
        });
}

// ── Write methods ─────────────────────────────────────────

void SolanaApi::sendTransaction(const QByteArray& serializedTx, bool skipPreflight,
                                const QString& preflightCommitment, int maxRetries) {
    const QString encoded = serializedTx.toBase64();

    QJsonObject options;
    options["encoding"] = "base64";
    options["skipPreflight"] = skipPreflight;
    options["preflightCommitment"] = preflightCommitment;
    if (maxRetries >= 0) {
        options["maxRetries"] = maxRetries;
    }

    QJsonArray params;
    params.append(encoded);
    params.append(options);

    postRpcHighPriority("sendTransaction", params, [this](const QJsonObject& root) {
        emit transactionSent(root["result"].toString());
    });
}

void SolanaApi::simulateTransaction(const QByteArray& serializedTx, bool sigVerify,
                                    bool replaceRecentBlockhash, const QString& commitment) {
    const QString encoded = serializedTx.toBase64();

    QJsonObject options;
    options["encoding"] = "base64";
    options["commitment"] = commitment;
    options["sigVerify"] = sigVerify;
    options["replaceRecentBlockhash"] = replaceRecentBlockhash;

    QJsonArray params;
    params.append(encoded);
    params.append(options);

    postRpcHighPriority("simulateTransaction", params, [this](const QJsonObject& root) {
        emit simulationReady(
            SimulationResponse::fromJson(root["result"].toObject()["value"].toObject()));
    });
}

// ── Low-priority methods (for background backfill) ────────

void SolanaApi::fetchSignaturesLowPriority(const QString& address, int limit,
                                           const QString& before) {
    QJsonObject options;
    options["limit"] = limit;
    if (!before.isEmpty()) {
        options["before"] = before;
    }

    QJsonArray params;
    params.append(address);
    params.append(options);

    postRpcLowPriority("getSignaturesForAddress", params, [this, address](const QJsonObject& root) {
        emit backfillSignaturesReady(address,
                                     SignatureInfo::fromJsonArray(root["result"].toArray()));
    });
}

void SolanaApi::fetchAccountInfoLowPriority(const QString& address, const QString& encoding) {
    QJsonObject options;
    options["encoding"] = encoding;

    QJsonArray params;
    params.append(address);
    params.append(options);

    postRpcLowPriority("getAccountInfo", params, [this, address](const QJsonObject& root) {
        const QJsonObject value = root["result"].toObject()["value"].toObject();
        if (value.isEmpty()) {
            emit backfillAccountInfoReady(address, QByteArray(), QString(), 0);
            return;
        }
        const QJsonArray dataArr = value["data"].toArray();
        QByteArray data = QByteArray::fromBase64(dataArr[0].toString().toUtf8());
        QString owner = value["owner"].toString();
        quint64 lamports = static_cast<quint64>(value["lamports"].toDouble());
        emit backfillAccountInfoReady(address, data, owner, lamports);
    });
}

void SolanaApi::fetchTransactionLowPriority(const QString& signature) {
    QJsonObject options;
    options["encoding"] = "jsonParsed";
    options["maxSupportedTransactionVersion"] = 0;

    QJsonArray params;
    params.append(signature);
    params.append(options);

    postRpcLowPriority("getTransaction", params, [this, signature](const QJsonObject& root) {
        emit backfillTransactionReady(signature,
                                      TransactionResponse::fromJson(root["result"].toObject()));
    });
}

void SolanaApi::logRpcStats() {
    if (m_totalRequests == 0 && m_total429s == 0) {
        return; // nothing to report
    }

    qDebug() << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━";
    qDebug() << "[RpcStats] 60s summary — total requests:" << m_totalRequests
             << "| 429s:" << m_total429s << "| active:" << m_activeRequests
             << "| queued: high=" << m_highQueue.size() << "normal=" << m_normalQueue.size()
             << "low=" << m_lowQueue.size();

    // Per-method request counts
    QStringList methods = m_requestCounts.keys();
    methods.sort();
    for (const auto& m : methods) {
        int count = m_requestCounts.value(m);
        int rate429 = m_429Counts.value(m, 0);
        int limit = m_methodLimits.value(m, DEFAULT_METHOD_LIMIT);
        qDebug() << "[RpcStats]  " << m << "requests:" << count << "429s:" << rate429
                 << "method-limit:" << limit << "/10s";
    }

    // Data volume
    qint64 windowBytes = dataUsedInWindow();
    qDebug() << "[RpcStats] Data(30s):" << (windowBytes / 1024) << "KB"
             << "/ 100MB";

    // Methods currently in backoff
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (auto it = m_methodBackoffUntil.constBegin(); it != m_methodBackoffUntil.constEnd(); ++it) {
        if (it.value() > now) {
            qDebug() << "[RpcStats] Backoff active:" << it.key()
                     << "remaining:" << (it.value() - now) << "ms";
        }
    }

    // Learned limits summary
    if (!m_methodLimits.isEmpty()) {
        QStringList limitParts;
        QStringList limitKeys = m_methodLimits.keys();
        limitKeys.sort();
        for (const auto& k : limitKeys) {
            limitParts << QString("%1=%2").arg(k).arg(m_methodLimits[k]);
        }
        qDebug() << "[RpcStats] Learned limits:" << limitParts.join(", ");
    }

    qDebug() << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━";

    // Reset counters for next interval
    m_totalRequests = 0;
    m_total429s = 0;
    m_requestCounts.clear();
    m_429Counts.clear();
}

void SolanaApi::flushQueues() {
    int discarded = m_highQueue.size() + m_normalQueue.size() + m_lowQueue.size();
    m_highQueue.clear();
    m_normalQueue.clear();
    m_lowQueue.clear();
    m_drainTimer.stop();
    if (discarded > 0) {
        qDebug() << "[SolanaApi] Flushed" << discarded << "queued requests"
                 << "(active in-flight:" << m_activeRequests << ")";
    }
}

int SolanaApi::totalQueueSize() const {
    return m_highQueue.size() + m_normalQueue.size() + m_lowQueue.size();
}
