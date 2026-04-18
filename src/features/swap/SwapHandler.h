#ifndef SWAPHANDLER_H
#define SWAPHANDLER_H

#include "models/Swap.h"
#include "services/model/JupiterQuote.h"
#include <QMap>
#include <QObject>
#include <QString>

class JupiterApi;
class SolanaApi;
class Signer;

class SwapHandler : public QObject {
    Q_OBJECT

  public:
    explicit SwapHandler(QObject* parent = nullptr);

    void setSolanaApi(SolanaApi* api);
    void setSigner(Signer* signer);
    void setWalletAddress(const QString& address);

    void refreshBalances();
    void invalidateQuote();
    void requestQuote(const QString& fromDisplay, const QString& toDisplay, const QString& amount);
    void executeSwap();

    bool quoteValid() const;
    bool swapInProgress() const;
    bool hasWalletAddress() const;

  signals:
    void tokenOptionsReady(const QList<SwapTokenOption>& fromOptions,
                           const QString& fromDefaultDisplay,
                           const QList<SwapTokenOption>& toOptions,
                           const QString& toDefaultDisplay);
    void quoteUpdated(const SwapQuoteView& view);
    void quoteCleared();
    void statusChanged(const QString& text, bool isError);
    void quoteValidityChanged(bool valid);
    void swapInProgressChanged(bool inProgress);

  private:
    enum class JupiterEndpoint { Quote, Swap, Unknown };

    void setQuoteValid(bool valid);
    void setSwapInProgress(bool inProgress);
    void onQuoteReady(const JupiterQuote& quote);
    void onSwapTransactionReady(const QByteArray& serializedTx);
    void onJupiterRequestFailed(const QString& endpoint, const QString& error);
    void onTransactionSent(const QString& signature);
    void onSolanaRequestFailed(const QString& method, const QString& error);

    QString mintForDisplay(const QString& display) const;
    int decimalsForMint(const QString& mint) const;
    QString symbolForMint(const QString& mint) const;
    static JupiterEndpoint endpointFromString(const QString& endpoint);

    SolanaApi* m_solanaApi = nullptr;
    JupiterApi* m_jupiterApi = nullptr;
    Signer* m_signer = nullptr;

    QString m_walletAddress;
    QMap<QString, QString> m_tokenDisplayToMint;

    JupiterQuote m_currentQuote;
    bool m_quoteValid = false;
    bool m_swapInProgress = false;
};

#endif // SWAPHANDLER_H
