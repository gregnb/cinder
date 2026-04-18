#ifndef TOKENMETADATASERVICE_H
#define TOKENMETADATASERVICE_H

#include <QNetworkAccessManager>
#include <QObject>
#include <QQueue>
#include <QTimer>

class SolanaApi;
class AvatarCache;

class TokenMetadataService : public QObject {
    Q_OBJECT

  public:
    explicit TokenMetadataService(SolanaApi* solanaApi, AvatarCache* avatarCache,
                                  QObject* parent = nullptr);

    // Scan DB for unfetched mints and begin resolving.
    void start();
    void pause();
    void resume();
    bool isPaused() const;

  signals:
    void metadataResolved(const QString& mint);

  private:
    void resolveNext();
    void onAccountData(const QString& address, const QByteArray& data, const QString& owner,
                       quint64 lamports);
    void fetchMetadataUri(const QString& mint, const QString& uri);
    void markFetched(const QString& mint);

    SolanaApi* m_solanaApi = nullptr;
    AvatarCache* m_avatarCache = nullptr;
    QNetworkAccessManager m_nam;

    void tryToken2022Fallback();
    void onMintAccountData(const QString& address, const QByteArray& data, const QString& owner,
                           quint64 lamports);
    void handleParsedMetadata(const QString& name, const QString& symbol, const QString& uri);

    QQueue<QString> m_mintQueue;
    QString m_currentMint;
    QString m_currentPda;
    bool m_resolving = false;

    QTimer m_delayTimer;
    QMetaObject::Connection m_connAccountInfo;
    bool m_paused = false;

    static constexpr int RESOLVE_DELAY_MS = 2000;
    static constexpr int URI_TIMEOUT_MS = 8000;
};

#endif // TOKENMETADATASERVICE_H
