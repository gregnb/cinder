#include "BackfillService.h"
#include "db/SyncStateDb.h"
#include "db/TransactionDb.h"
#include "services/SolanaApi.h"
#include "tx/TxParseUtils.h"
#include <QDebug>
#include <QJsonDocument>

BackfillService::BackfillService(SolanaApi* solanaApi, QObject* parent)
    : QObject(parent), m_solanaApi(solanaApi) {
    m_startDelay.setSingleShot(true);
    m_startDelay.setInterval(BACKFILL_START_DELAY_MS);
    connect(&m_startDelay, &QTimer::timeout, this, &BackfillService::fetchNextPage);

    m_pageDelay.setSingleShot(true);
    m_pageDelay.setInterval(BACKFILL_PAGE_DELAY_MS);
    connect(&m_pageDelay, &QTimer::timeout, this, &BackfillService::fetchNextPage);
}

void BackfillService::setWalletAddress(const QString& address) {
    if (address == m_address) {
        return;
    }

    stop();
    m_address = address;

    if (!m_address.isEmpty()) {
        start();
    }
}

void BackfillService::start() {
    if (m_address.isEmpty() || m_running) {
        return;
    }

    // Check if backfill already completed for this address
    if (SyncStateDb::isBackfillComplete(m_address)) {
        qDebug() << "[BackfillService] Backfill already complete for" << m_address;
        return;
    }

    m_running = true;
    emit started();

    // Load persisted cursor
    m_cursor = SyncStateDb::oldestFetchedSignature(m_address);
    qDebug() << "[BackfillService] Starting for" << m_address
             << "cursor:" << (m_cursor.isEmpty() ? "(none)" : m_cursor);

    connectSignals();
    m_startDelay.start();
}

void BackfillService::stop() {
    m_startDelay.stop();
    m_pageDelay.stop();
    disconnectSignals();
    m_running = false;
    m_pageInProgress = false;
    m_pendingSignatures.clear();
    m_pendingTxFetches = 0;
    m_paused = false;
}

void BackfillService::pause() {
    if (m_paused) {
        return;
    }
    m_paused = true;
    m_startDelay.stop();
    m_pageDelay.stop();
    qDebug() << "[BackfillService] Paused";
}

void BackfillService::resume() {
    if (!m_paused) {
        return;
    }
    m_paused = false;
    if (m_running && !m_pageInProgress) {
        m_pageDelay.start();
    }
    qDebug() << "[BackfillService] Resumed";
}

bool BackfillService::isPaused() const { return m_paused; }

void BackfillService::connectSignals() {
    disconnectSignals();

    m_connSignatures = connect(m_solanaApi, &SolanaApi::backfillSignaturesReady, this,
                               &BackfillService::handleSignatures);
    m_connTransaction = connect(m_solanaApi, &SolanaApi::backfillTransactionReady, this,
                                &BackfillService::handleTransaction);
    m_connError = connect(m_solanaApi, &SolanaApi::requestFailed, this,
                          [this](const QString& method, const QString& error) {
                              if (!m_running) {
                                  return;
                              }

                              // We only handle signature-fetch errors here.
                              // getTransaction 429s are re-enqueued transparently by
                              // SolanaApi and will eventually complete.
                              if (method == "getSignaturesForAddress" && m_pageInProgress &&
                                  m_pendingTxFetches == 0) {
                                  qDebug() << "[BackfillService] getSignaturesForAddress failed:"
                                           << error << "— retrying after delay";
                                  m_pageInProgress = false;
                                  m_pageDelay.start();
                              }
                          });
}

void BackfillService::disconnectSignals() {
    disconnect(m_connSignatures);
    disconnect(m_connTransaction);
    disconnect(m_connError);
}

void BackfillService::fetchNextPage() {
    if (!m_running || m_pageInProgress || m_address.isEmpty()) {
        return;
    }

    m_pageInProgress = true;
    m_pendingSignatures.clear();
    m_pendingTxFetches = 0;
    m_newInCurrentPage = 0;

    qDebug() << "[BackfillService] Fetching page — cursor:"
             << (m_cursor.isEmpty() ? "(start)" : m_cursor);

    m_solanaApi->fetchSignaturesLowPriority(m_address, BACKFILL_SIGNATURE_LIMIT, m_cursor);
}

void BackfillService::handleSignatures(const QString& addr, const QList<SignatureInfo>& sigs) {
    if (addr != m_address || !m_pageInProgress) {
        return;
    }

    // If we have our own pending signatures, this response is from SyncService, not us.
    // We detect our response by checking if this is a page from our cursor position.
    // Since SyncService fetches only 25 sigs (no cursor) and we fetch 1000 with a cursor,
    // we distinguish by checking the batch size or the cursor match.
    // However, both use the same signal. We use the pendingSignatures set to track ours.

    // If we already have pending fetches (a page is in flight), ignore duplicate responses
    if (m_pendingTxFetches > 0) {
        return;
    }

    // End condition: fewer results than requested means we've reached the beginning
    if (sigs.isEmpty()) {
        qDebug() << "[BackfillService] Backfill complete — no more signatures";
        m_pageInProgress = false;
        SyncStateDb::setBackfillComplete(m_address, true);
        m_running = false;
        disconnectSignals();
        emit backfillComplete();
        return;
    }

    // Update cursor to the last (oldest) signature in this batch
    m_cursor = sigs.last().signature;
    SyncStateDb::setOldestFetchedSignature(m_address, m_cursor);

    // Filter out sigs already in DB
    for (const auto& sig : sigs) {
        if (!TransactionDb::hasTransaction(sig.signature)) {
            m_pendingSignatures.insert(sig.signature);
        }
    }

    m_pendingTxFetches = m_pendingSignatures.size();

    qDebug() << "[BackfillService] Page received:" << sigs.size() << "sigs," << m_pendingTxFetches
             << "new";

    if (m_pendingTxFetches == 0) {
        // All already in DB — continue to next page
        m_pageInProgress = false;

        // Check if we've reached the end
        if (sigs.size() < BACKFILL_SIGNATURE_LIMIT) {
            qDebug() << "[BackfillService] Backfill complete — last page had" << sigs.size()
                     << "sigs";
            SyncStateDb::setBackfillComplete(m_address, true);
            m_running = false;
            disconnectSignals();
            emit backfillComplete();
        } else {
            emit progressUpdated(SyncStateDb::totalFetched(m_address));
            m_pageDelay.start();
        }
        return;
    }

    // Fetch each new transaction at low priority
    for (const auto& sig : m_pendingSignatures) {
        m_solanaApi->fetchTransactionLowPriority(sig);
    }
}

void BackfillService::handleTransaction(const QString& signature, const TransactionResponse& tx) {
    if (!m_pendingSignatures.contains(signature)) {
        return;
    }
    m_pendingSignatures.remove(signature);

    // Parse and store
    QList<Activity> activities = TxParseUtils::extractActivities(tx, m_address);
    QString rawJson = QJsonDocument(tx.rawJson).toJson(QJsonDocument::Compact);
    TransactionDb::insertTransaction(signature, tx.slot, tx.blockTime, rawJson,
                                     static_cast<int>(tx.meta.fee), tx.meta.hasError, activities);

    m_newInCurrentPage++;
    m_pendingTxFetches--;

    qDebug() << "[BackfillService] Stored tx" << signature.left(12) << "..."
             << "| activities:" << activities.size() << "| remaining:" << m_pendingTxFetches;

    onTxFetchDone();
}

void BackfillService::onTxFetchDone() {
    if (m_pendingTxFetches > 0) {
        return;
    }

    // Page complete
    m_pageInProgress = false;

    // Update persistent counter
    int total = SyncStateDb::totalFetched(m_address) + m_newInCurrentPage;
    SyncStateDb::setTotalFetched(m_address, total);

    qDebug() << "[BackfillService] Page complete —" << m_newInCurrentPage
             << "new txs, total:" << total;

    emit pageComplete(m_newInCurrentPage);
    emit progressUpdated(total);

    if (!m_running) {
        return;
    }

    // Schedule next page
    m_pageDelay.start();
}
