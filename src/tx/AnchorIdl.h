#ifndef ANCHORIDL_H
#define ANCHORIDL_H

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QString>
#include <sodium.h>

namespace AnchorIdl {

    struct AccountMeta {
        QString name;
        bool writable = false;
        bool signer = false;
    };

    struct IdlInstruction {
        QString name;             // e.g. "route", "swap"
        QByteArray discriminator; // 8 bytes
        QList<AccountMeta> accounts;
    };

    struct Idl {
        QString address; // program ID (base58)
        QString name;    // e.g. "jupiter_v6"
        QString version;
        QList<IdlInstruction> instructions;
        QMap<QByteArray, int> discriminatorMap;

        bool isValid() const { return !instructions.isEmpty(); }

        const IdlInstruction* findInstruction(const QByteArray& ixData) const {
            if (ixData.size() < 8)
                return nullptr;
            QByteArray disc = ixData.left(8);
            auto it = discriminatorMap.find(disc);
            if (it == discriminatorMap.end())
                return nullptr;
            return &instructions[it.value()];
        }

        void buildIndex() {
            discriminatorMap.clear();
            for (int i = 0; i < instructions.size(); ++i)
                discriminatorMap[instructions[i].discriminator] = i;
        }

        // "jupiter_v6" -> "Jupiter V6"
        QString displayName() const {
            QString result = name;
            result.replace('_', ' ');
            bool nextUpper = true;
            for (int i = 0; i < result.size(); ++i) {
                if (nextUpper && result[i].isLetter()) {
                    result[i] = result[i].toUpper();
                    nextUpper = false;
                } else if (result[i] == ' ') {
                    nextUpper = true;
                }
            }
            return result;
        }

        // Compute Anchor discriminator: sha256("global:<name>")[0..8]
        static QByteArray computeDiscriminator(const QString& instructionName) {
            QByteArray preimage = "global:" + instructionName.toUtf8();
            unsigned char hash[crypto_hash_sha256_BYTES];
            crypto_hash_sha256(hash, reinterpret_cast<const unsigned char*>(preimage.constData()),
                               static_cast<unsigned long long>(preimage.size()));
            return QByteArray(reinterpret_cast<const char*>(hash), 8);
        }

        static Idl fromJson(const QJsonObject& json) {
            Idl idl;
            idl.address = json["address"].toString();

            QJsonObject meta = json["metadata"].toObject();
            idl.name = meta["name"].toString();
            idl.version = meta["version"].toString();

            // Legacy format: name/version at top level
            if (idl.name.isEmpty())
                idl.name = json["name"].toString();
            if (idl.version.isEmpty())
                idl.version = json["version"].toString();

            for (const auto& v : json["instructions"].toArray()) {
                QJsonObject ixObj = v.toObject();
                IdlInstruction ix;
                ix.name = ixObj["name"].toString();

                // v0.30+ has explicit discriminator array
                if (ixObj.contains("discriminator")) {
                    QJsonArray discArr = ixObj["discriminator"].toArray();
                    QByteArray disc(discArr.size(), '\0');
                    for (int i = 0; i < discArr.size(); ++i)
                        disc[i] = static_cast<char>(discArr[i].toInt());
                    ix.discriminator = disc;
                } else {
                    // Legacy: compute from name
                    ix.discriminator = computeDiscriminator(ix.name);
                }

                // Accounts: v0.30+ uses writable/signer, legacy uses isMut/isSigner
                for (const auto& accV : ixObj["accounts"].toArray()) {
                    QJsonObject accObj = accV.toObject();
                    AccountMeta am;
                    am.name = accObj["name"].toString();
                    am.writable = accObj.contains("writable") ? accObj["writable"].toBool()
                                                              : accObj["isMut"].toBool();
                    am.signer = accObj.contains("signer") ? accObj["signer"].toBool()
                                                          : accObj["isSigner"].toBool();
                    ix.accounts.append(am);
                }

                idl.instructions.append(ix);
            }

            idl.buildIndex();
            return idl;
        }
    };

} // namespace AnchorIdl

#endif // ANCHORIDL_H
