#ifndef AICLIENT_H
#define AICLIENT_H

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QString>

class AiClient : public QObject {
    Q_OBJECT

  public:
    explicit AiClient(QObject* parent = nullptr);

    void sendMessage(const QString& token, const QString& model, const QString& userMessage,
                     int maxTokens = 1024);
    void cancel();
    bool isBusy() const;

  signals:
    void responseReady(const QString& text);
    void requestFailed(const QString& error);

  private:
    QNetworkAccessManager m_nam;
    QNetworkReply* m_activeReply = nullptr;
};

#endif // AICLIENT_H
