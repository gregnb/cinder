#ifndef IDLREGISTRY_H
#define IDLREGISTRY_H

#include "tx/AnchorIdl.h"
#include <QMap>
#include <QObject>
#include <QSet>

class SolanaApi;

class IdlRegistry : public QObject {
    Q_OBJECT
  public:
    explicit IdlRegistry(SolanaApi* api, QObject* parent = nullptr);

    // Synchronous lookup: returns cached/bundled IDL or nullptr.
    const AnchorIdl::Idl* lookup(const QString& programId) const;

    // Lookup + trigger async on-chain fetch if missing.
    // Returns IDL if available synchronously, nullptr if fetch needed.
    // Emits idlReady() when fetch completes.
    const AnchorIdl::Idl* resolve(const QString& programId);

    // IDL-aware program name: IDL displayName → built-in names → truncated address.
    QString friendlyName(const QString& programId) const;

  signals:
    void idlReady(const QString& programId);

  private:
    void loadBundledIdls();
    void loadCachedIdls();
    void fetchOnChain(const QString& programId);

    static QByteArray deriveIdlAddress(const QByteArray& programId32);
    static QByteArray findProgramAddress(const QList<QByteArray>& seeds,
                                         const QByteArray& programId32);

    SolanaApi* m_api;
    QMap<QString, AnchorIdl::Idl> m_idls;
    QSet<QString> m_pendingFetches;
    QSet<QString> m_noIdlPrograms;
};

#endif // IDLREGISTRY_H
