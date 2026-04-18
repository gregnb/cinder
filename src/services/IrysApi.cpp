#include "services/IrysApi.h"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace {
    constexpr char kIrysPriceBaseUrl[] = "https://node1.irys.xyz/price/solana/";
} // namespace

IrysApi::IrysApi(QObject* parent) : QObject(parent) {}

void IrysApi::fetchSolanaStoragePriceLamports(qint64 fileSize,
                                              const std::function<void(quint64)>& onSuccess,
                                              const std::function<void(const QString&)>& onError) {
    if (fileSize <= 0) {
        if (onError) {
            onError("invalid_file_size");
        }
        return;
    }

    const QUrl url(QString(kIrysPriceBaseUrl) + QString::number(fileSize));
    QNetworkReply* reply = m_nam.get(QNetworkRequest(url));
    QObject::connect(reply, &QNetworkReply::finished, this, [reply, onSuccess, onError]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            if (onError) {
                onError(reply->errorString());
            }
            return;
        }

        bool ok = false;
        const quint64 lamports = reply->readAll().trimmed().toULongLong(&ok);
        if (!ok) {
            if (onError) {
                onError("invalid_irys_price_response");
            }
            return;
        }

        if (onSuccess) {
            onSuccess(lamports);
        }
    });
}
