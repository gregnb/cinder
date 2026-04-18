#ifndef IRYSAPI_H
#define IRYSAPI_H

#include <QNetworkAccessManager>
#include <QObject>
#include <functional>

class IrysApi : public QObject {
    Q_OBJECT

  public:
    explicit IrysApi(QObject* parent = nullptr);

    void fetchSolanaStoragePriceLamports(qint64 fileSize,
                                         const std::function<void(quint64)>& onSuccess,
                                         const std::function<void(const QString&)>& onError);

  private:
    QNetworkAccessManager m_nam;
};

#endif // IRYSAPI_H
