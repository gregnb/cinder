#include "TokenMetadataService.h"
#include "crypto/PdaDerivation.h"
#include "db/TokenAccountDb.h"
#include "services/AvatarCache.h"
#include "services/SolanaApi.h"
#include "tx/KnownTokens.h"
#include "tx/MetaplexMetadata.h"
#include "tx/Token2022Metadata.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>

TokenMetadataService::TokenMetadataService(SolanaApi* solanaApi, AvatarCache* avatarCache,
                                           QObject* parent)
    : QObject(parent), m_solanaApi(solanaApi), m_avatarCache(avatarCache) {
    m_delayTimer.setSingleShot(true);
    m_delayTimer.setInterval(RESOLVE_DELAY_MS);
    connect(&m_delayTimer, &QTimer::timeout, this, &TokenMetadataService::resolveNext);
}

void TokenMetadataService::pause() {
    if (m_paused) {
        return;
    }
    m_paused = true;
    m_delayTimer.stop();
    qDebug() << "[TokenMetadata] Paused";
}

void TokenMetadataService::resume() {
    if (!m_paused) {
        return;
    }
    m_paused = false;
    if (m_resolving && !m_mintQueue.isEmpty()) {
        m_delayTimer.start();
    }
    qDebug() << "[TokenMetadata] Resumed";
}

bool TokenMetadataService::isPaused() const { return m_paused; }

void TokenMetadataService::start() {
    // Insert stub rows for mints found in transactions but not yet in tokens table
    int inserted = TokenAccountDb::insertMissingTransactionMints();
    if (inserted > 0) {
        qDebug() << "[TokenMetadata] Inserted" << inserted << "missing transaction mints";
    }

    QStringList mints = TokenAccountDb::getUnfetchedMints();
    if (mints.isEmpty()) {
        qDebug() << "[TokenMetadata] No unfetched mints";
        return;
    }

    for (const auto& mint : mints) {
        if (!m_mintQueue.contains(mint)) {
            m_mintQueue.enqueue(mint);
        }
    }

    qDebug() << "[TokenMetadata] Queued" << m_mintQueue.size() << "mints for resolution";

    if (!m_resolving) {
        resolveNext();
    }
}

void TokenMetadataService::resolveNext() {
    if (m_mintQueue.isEmpty()) {
        m_resolving = false;
        qDebug() << "[TokenMetadata] All mints resolved";
        return;
    }

    m_resolving = true;
    m_currentMint = m_mintQueue.dequeue();

    // Native SOL uses WSOL mint address but we always want "SOL" / "Solana"
    if (m_currentMint == WSOL_MINT) {
        TokenAccountDb::updateTokenMetadata(m_currentMint, "Solana", "SOL",
                                            ":/icons/tokens/sol.png");
        emit metadataResolved(m_currentMint);
        m_delayTimer.start();
        return;
    }

    // Derive Metaplex metadata PDA
    m_currentPda = PdaDerivation::metaplexMetadataPda(m_currentMint);
    if (m_currentPda.isEmpty()) {
        qDebug() << "[TokenMetadata] Failed to derive PDA for" << m_currentMint.left(12);
        markFetched(m_currentMint);
        m_delayTimer.start();
        return;
    }

    qDebug() << "[TokenMetadata] Resolving" << m_currentMint.left(12) << "..."
             << "PDA:" << m_currentPda.left(12) << "...";

    // Phase 1: Try Metaplex metadata PDA
    disconnect(m_connAccountInfo);
    m_connAccountInfo = connect(m_solanaApi, &SolanaApi::backfillAccountInfoReady, this,
                                &TokenMetadataService::onAccountData);

    m_solanaApi->fetchAccountInfoLowPriority(m_currentPda);
}

void TokenMetadataService::onAccountData(const QString& address, const QByteArray& data,
                                         const QString& owner, quint64 lamports) {
    Q_UNUSED(owner)
    Q_UNUSED(lamports)

    if (address != m_currentPda) {
        return;
    }

    disconnect(m_connAccountInfo);

    if (data.isEmpty()) {
        // No Metaplex metadata — try Token-2022 fallback
        qDebug() << "[TokenMetadata] No Metaplex PDA for" << m_currentMint.left(12)
                 << "— trying Token-2022 fallback";
        tryToken2022Fallback();
        return;
    }

    MetaplexMetadata meta = MetaplexMetadata::fromAccountData(data);
    if (!meta.valid) {
        qDebug() << "[TokenMetadata] Failed to parse Borsh for" << m_currentMint.left(12);
        tryToken2022Fallback();
        return;
    }

    qDebug() << "[TokenMetadata] Metaplex:" << m_currentMint.left(12) << "..."
             << "name=" << meta.name << "symbol=" << meta.symbol << "uri=" << meta.uri.left(60);

    handleParsedMetadata(meta.name, meta.symbol, meta.uri);
}

void TokenMetadataService::tryToken2022Fallback() {
    // Phase 2: Fetch the mint account itself and try Token-2022 extension parsing
    disconnect(m_connAccountInfo);
    m_connAccountInfo = connect(m_solanaApi, &SolanaApi::backfillAccountInfoReady, this,
                                &TokenMetadataService::onMintAccountData);

    m_solanaApi->fetchAccountInfoLowPriority(m_currentMint);
}

void TokenMetadataService::onMintAccountData(const QString& address, const QByteArray& data,
                                             const QString& owner, quint64 lamports) {
    Q_UNUSED(owner)
    Q_UNUSED(lamports)

    if (address != m_currentMint) {
        return;
    }

    disconnect(m_connAccountInfo);

    if (data.isEmpty() || data.size() <= 82) {
        qDebug() << "[TokenMetadata] No Token-2022 extensions for" << m_currentMint.left(12);
        markFetched(m_currentMint);
        m_delayTimer.start();
        return;
    }

    Token2022Metadata meta = Token2022Metadata::fromMintAccountData(data);
    if (!meta.valid) {
        qDebug() << "[TokenMetadata] Failed to parse Token-2022 for" << m_currentMint.left(12);
        markFetched(m_currentMint);
        m_delayTimer.start();
        return;
    }

    qDebug() << "[TokenMetadata] Token-2022:" << m_currentMint.left(12) << "..."
             << "name=" << meta.name << "symbol=" << meta.symbol << "uri=" << meta.uri.left(60);

    handleParsedMetadata(meta.name, meta.symbol, meta.uri);
}

void TokenMetadataService::handleParsedMetadata(const QString& name, const QString& symbol,
                                                const QString& uri) {
    if (uri.isEmpty()) {
        // No URI — store name/symbol but no image
        TokenAccountDb::updateTokenMetadata(m_currentMint,
                                            name.isEmpty() ? m_currentMint.left(6) : name,
                                            symbol.isEmpty() ? m_currentMint.left(6) : symbol, "");
        emit metadataResolved(m_currentMint);
        m_delayTimer.start();
        return;
    }

    // Fetch the metadata JSON from the URI to get the image URL
    fetchMetadataUri(m_currentMint, uri);
}

void TokenMetadataService::fetchMetadataUri(const QString& mint, const QString& uri) {
    QUrl url(uri);
    QNetworkRequest req(url);
    req.setTransferTimeout(URI_TIMEOUT_MS);
    req.setHeader(QNetworkRequest::UserAgentHeader, "Cinder/0.1.0");

    QNetworkReply* reply = m_nam.get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply, mint]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            qDebug() << "[TokenMetadata] URI fetch failed for" << mint.left(12) << ":"
                     << reply->errorString();
            markFetched(mint);
            m_delayTimer.start();
            return;
        }

        QByteArray body = reply->readAll();
        if (body.size() > 512LL * 1024) {
            qDebug() << "[TokenMetadata] URI response too large for" << mint.left(12);
            markFetched(mint);
            m_delayTimer.start();
            return;
        }

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(body, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            qDebug() << "[TokenMetadata] URI JSON parse failed for" << mint.left(12);
            markFetched(mint);
            m_delayTimer.start();
            return;
        }

        QJsonObject json = doc.object();
        QString name = json["name"].toString().trimmed();
        QString symbol = json["symbol"].toString().trimmed();
        QString image = json["image"].toString().trimmed();

        qDebug() << "[TokenMetadata] Resolved" << mint.left(12) << "..."
                 << "→ name=" << name << "symbol=" << symbol << "image=" << image.left(60);

        // Update DB
        TokenAccountDb::updateTokenMetadata(mint, name.isEmpty() ? mint.left(6) : name,
                                            symbol.isEmpty() ? mint.left(6) : symbol, image);

        // Trigger image download via AvatarCache
        if (!image.isEmpty() && m_avatarCache) {
            m_avatarCache->get(image);
        }

        emit metadataResolved(mint);
        m_delayTimer.start();
    });
}

void TokenMetadataService::markFetched(const QString& mint) {
    // Mark as fetched even on failure to avoid re-trying forever.
    TokenAccountDb::updateTokenMetadata(mint, mint.left(6), mint.left(6), "");
}
