#include "IdlRegistry.h"
#include "db/IdlDb.h"
#include "services/SolanaApi.h"
#include "tx/Base58.h"
#include "tx/ProgramIds.h"
#include "tx/TxClassifier.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <sodium.h>
#include <zlib.h>

// ── Constructor ─────────────────────────────────────────

IdlRegistry::IdlRegistry(SolanaApi* api, QObject* parent) : QObject(parent), m_api(api) {
    loadBundledIdls();
    loadCachedIdls();
}

// ── Bundled IDLs ────────────────────────────────────────

void IdlRegistry::loadBundledIdls() {
    static const QStringList files = {
        ":/idl/jupiter_v6.json",   ":/idl/raydium_clmm.json",  ":/idl/raydium_cp_swap.json",
        ":/idl/meteora_dlmm.json", ":/idl/drift_v2.json",      ":/idl/marinade.json",
        ":/idl/tensor_tswap.json", ":/idl/magic_eden_m2.json", ":/idl/squads_v4.json",
        ":/idl/pump_fun.json",
    };

    for (const QString& path : files) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            qWarning() << "[IdlRegistry] Could not open bundled IDL:" << path;
            continue;
        }
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        AnchorIdl::Idl idl = AnchorIdl::Idl::fromJson(doc.object());
        if (idl.isValid() && !idl.address.isEmpty()) {
            m_idls[idl.address] = idl;
        }
    }

    qDebug() << "[IdlRegistry] Loaded" << m_idls.size() << "bundled IDLs";
}

// ── Cached IDLs ─────────────────────────────────────────

void IdlRegistry::loadCachedIdls() {
    const auto entries = IdlDb::getAll();
    int loaded = 0;
    for (const auto& pair : entries) {
        const QString& programId = pair.first;
        const QString& json = pair.second;

        if (m_idls.contains(programId)) {
            continue; // bundled takes precedence
        }

        if (json.isEmpty()) {
            m_noIdlPrograms.insert(programId);
            continue;
        }

        QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
        AnchorIdl::Idl idl = AnchorIdl::Idl::fromJson(doc.object());
        if (idl.isValid()) {
            m_idls[programId] = idl;
            ++loaded;
        }
    }

    if (loaded > 0) {
        qDebug() << "[IdlRegistry] Loaded" << loaded << "cached IDLs";
    }
}

// ── Lookup ──────────────────────────────────────────────

const AnchorIdl::Idl* IdlRegistry::lookup(const QString& programId) const {
    auto it = m_idls.find(programId);
    return (it != m_idls.end()) ? &it.value() : nullptr;
}

const AnchorIdl::Idl* IdlRegistry::resolve(const QString& programId) {
    if (auto* idl = lookup(programId)) {
        return idl;
    }

    if (m_noIdlPrograms.contains(programId)) {
        return nullptr;
    }

    if (m_pendingFetches.contains(programId)) {
        return nullptr;
    }

    fetchOnChain(programId);
    return nullptr;
}

// ── Friendly name ───────────────────────────────────────

QString IdlRegistry::friendlyName(const QString& programId) const {
    if (auto* idl = lookup(programId)) {
        return idl->displayName();
    }

    // Built-in program names
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
    return programId.left(8) + "..." + programId.right(4);
}

// ── PDA derivation ──────────────────────────────────────

QByteArray IdlRegistry::findProgramAddress(const QList<QByteArray>& seeds,
                                           const QByteArray& programId32) {
    if (programId32.size() != 32) {
        return {};
    }

    // Try nonces 255..0 until point is NOT on Ed25519 curve
    for (int nonce = 255; nonce >= 0; --nonce) {
        crypto_hash_sha256_state state;
        crypto_hash_sha256_init(&state);

        for (const auto& s : seeds) {
            crypto_hash_sha256_update(&state, reinterpret_cast<const unsigned char*>(s.constData()),
                                      static_cast<unsigned long long>(s.size()));
        }

        unsigned char nonceByte = static_cast<unsigned char>(nonce);
        crypto_hash_sha256_update(&state, &nonceByte, 1);

        crypto_hash_sha256_update(
            &state, reinterpret_cast<const unsigned char*>(programId32.constData()), 32);

        const char* suffix = "ProgramDerivedAddress";
        crypto_hash_sha256_update(&state, reinterpret_cast<const unsigned char*>(suffix), 21);

        unsigned char hash[32];
        crypto_hash_sha256_final(&state, hash);

        // Must NOT be a valid Ed25519 point
        if (crypto_core_ed25519_is_valid_point(hash) == 0) {
            return QByteArray(reinterpret_cast<const char*>(hash), 32);
        }
    }

    return {};
}

QByteArray IdlRegistry::deriveIdlAddress(const QByteArray& programId32) {
    // Step 1: base = findProgramAddress([], programId)
    QByteArray base = findProgramAddress({}, programId32);
    if (base.isEmpty()) {
        return {};
    }

    // Step 2: createWithSeed(base, "anchor:idl", programId)
    // = sha256(base || seed || programId)
    crypto_hash_sha256_state state;
    crypto_hash_sha256_init(&state);
    crypto_hash_sha256_update(&state, reinterpret_cast<const unsigned char*>(base.constData()), 32);

    QByteArray seed = "anchor:idl";
    crypto_hash_sha256_update(&state, reinterpret_cast<const unsigned char*>(seed.constData()),
                              static_cast<unsigned long long>(seed.size()));

    crypto_hash_sha256_update(&state,
                              reinterpret_cast<const unsigned char*>(programId32.constData()), 32);

    unsigned char hash[32];
    crypto_hash_sha256_final(&state, hash);

    return QByteArray(reinterpret_cast<const char*>(hash), 32);
}

// ── On-chain fetch ──────────────────────────────────────

void IdlRegistry::fetchOnChain(const QString& programId) {
    QByteArray programId32 = Base58::decode(programId);
    if (programId32.size() != 32) {
        m_noIdlPrograms.insert(programId);
        return;
    }

    QByteArray idlAddr = deriveIdlAddress(programId32);
    if (idlAddr.isEmpty()) {
        m_noIdlPrograms.insert(programId);
        return;
    }

    QString idlAddrB58 = Base58::encode(idlAddr);
    m_pendingFetches.insert(programId);

    // Connect to accountInfoReady for this specific address
    auto conn = std::make_shared<QMetaObject::Connection>();
    auto errConn = std::make_shared<QMetaObject::Connection>();

    *conn = connect(
        m_api, &SolanaApi::accountInfoReady, this,
        [this, conn, errConn, programId, idlAddrB58](const QString& address, const QByteArray& data,
                                                     const QString&, quint64) {
            if (address != idlAddrB58) {
                return;
            }

            disconnect(*conn);
            disconnect(*errConn);
            m_pendingFetches.remove(programId);

            if (data.isEmpty() || data.size() < 44) {
                m_noIdlPrograms.insert(programId);
                IdlDb::markNoIdl(programId);
                return;
            }

            // Layout: [8-byte disc][32-byte authority][4-byte len][zlib data]
            quint32 dataLen =
                qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(data.constData() + 40));

            if (44 + static_cast<int>(dataLen) > data.size()) {
                m_noIdlPrograms.insert(programId);
                IdlDb::markNoIdl(programId);
                return;
            }

            QByteArray compressed = data.mid(44, static_cast<int>(dataLen));

            // zlib decompress
            uLongf uncompLen = dataLen * 20;
            QByteArray uncompressed(static_cast<int>(uncompLen), '\0');

            int ret = uncompress(reinterpret_cast<Bytef*>(uncompressed.data()), &uncompLen,
                                 reinterpret_cast<const Bytef*>(compressed.constData()),
                                 static_cast<uLong>(compressed.size()));

            // Retry with larger buffer if needed
            if (ret == Z_BUF_ERROR) {
                uncompLen = dataLen * 100;
                uncompressed.resize(static_cast<int>(uncompLen));
                ret = uncompress(reinterpret_cast<Bytef*>(uncompressed.data()), &uncompLen,
                                 reinterpret_cast<const Bytef*>(compressed.constData()),
                                 static_cast<uLong>(compressed.size()));
            }

            if (ret != Z_OK) {
                qWarning() << "[IdlRegistry] zlib decompress failed for" << programId
                           << "ret:" << ret;
                m_noIdlPrograms.insert(programId);
                IdlDb::markNoIdl(programId);
                return;
            }

            uncompressed.truncate(static_cast<int>(uncompLen));

            QJsonParseError parseErr;
            QJsonDocument doc = QJsonDocument::fromJson(uncompressed, &parseErr);
            if (parseErr.error != QJsonParseError::NoError) {
                qWarning() << "[IdlRegistry] JSON parse failed for" << programId;
                m_noIdlPrograms.insert(programId);
                IdlDb::markNoIdl(programId);
                return;
            }

            AnchorIdl::Idl idl = AnchorIdl::Idl::fromJson(doc.object());
            if (!idl.isValid()) {
                m_noIdlPrograms.insert(programId);
                IdlDb::markNoIdl(programId);
                return;
            }

            // Ensure address is set
            if (idl.address.isEmpty()) {
                idl.address = programId;
            }

            // Cache in DB
            IdlDb::upsertIdl(programId, idl.name, QString::fromUtf8(uncompressed));

            // Store in memory
            m_idls[programId] = idl;

            qDebug() << "[IdlRegistry] Fetched IDL for" << programId << "(" << idl.name << ","
                     << idl.instructions.size() << "instructions)";

            emit idlReady(programId);
        });

    *errConn =
        connect(m_api, &SolanaApi::requestFailed, this,
                [this, conn, errConn, programId](const QString& method, const QString& error) {
                    if (method != "getAccountInfo") {
                        return;
                    }

                    disconnect(*conn);
                    disconnect(*errConn);
                    m_pendingFetches.remove(programId);
                    m_noIdlPrograms.insert(programId);
                    IdlDb::markNoIdl(programId);

                    qDebug() << "[IdlRegistry] Failed to fetch IDL for" << programId << ":"
                             << error;
                });

    m_api->fetchAccountInfo(idlAddrB58);
}
