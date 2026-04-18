#ifndef JUPITERAPI_H
#define JUPITERAPI_H

#include "services/model/JupiterQuote.h"
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>

class JupiterApi : public QObject {
    Q_OBJECT

  public:
    explicit JupiterApi(QObject* parent = nullptr);

    // Fetch a swap quote from Jupiter aggregator.
    void fetchQuote(const QString& inputMint, const QString& outputMint, quint64 amount,
                    int slippageBps = 50);

    // Build a serialized swap transaction from a quote.
    void fetchSwapTransaction(const QJsonObject& quoteResponse, const QString& userPublicKey);

  signals:
    void quoteReady(const JupiterQuote& quote);
    void swapTransactionReady(const QByteArray& serializedTx, quint64 lastValidBlockHeight);
    void requestFailed(const QString& endpoint, const QString& error);

  private:
    void httpGet(const QUrl& url, const std::function<void(const QJsonObject&)>& onSuccess,
                 const QString& tag);
    void httpPost(const QUrl& url, const QJsonObject& body,
                  const std::function<void(const QJsonObject&)>& onSuccess, const QString& tag);

    QNetworkAccessManager m_nam;
    static const QString BASE_URL;
};

#endif // JUPITERAPI_H
