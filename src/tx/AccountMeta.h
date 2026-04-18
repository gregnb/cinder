#ifndef ACCOUNTMETA_H
#define ACCOUNTMETA_H

#include <QString>

struct AccountMeta {
    QString pubkey;
    bool isSigner = false;
    bool isWritable = false;

    static AccountMeta writable(const QString& pubkey, bool signer = false) {
        return {pubkey, signer, true};
    }

    static AccountMeta readonly(const QString& pubkey, bool signer = false) {
        return {pubkey, signer, false};
    }
};

#endif // ACCOUNTMETA_H
