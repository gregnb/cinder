#ifndef SWAPPAGE_H
#define SWAPPAGE_H

#include "crypto/Keypair.h"
#include <QTimer>
#include <QWidget>

class SolanaApi;
class Signer;
class TokenDropdown;
class AmountInput;
class QLabel;
class QPushButton;
class SwapHandler;

struct SwapTokenOption;
struct SwapQuoteView;

class SwapPage : public QWidget {
    Q_OBJECT

  public:
    explicit SwapPage(QWidget* parent = nullptr);

    void setSolanaApi(SolanaApi* api);
    void setKeypair(const Keypair& kp);
    void setSigner(Signer* signer);
    void setWalletAddress(const QString& address);
    void refreshBalances();

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private:
    void buildUI();
    void requestQuote();
    void executeSwap();
    void swapDirection();
    void updateSwapButton();
    void showStatus(const QString& text, bool isError = false);
    void stopRefreshTimers();
    void applyTokenOptions(const QList<SwapTokenOption>& fromOptions,
                           const QString& fromDefaultDisplay,
                           const QList<SwapTokenOption>& toOptions,
                           const QString& toDefaultDisplay);
    void applyQuoteView(const SwapQuoteView& view);

    SwapHandler* m_handler = nullptr;
    QString m_walletAddress;

    TokenDropdown* m_fromToken = nullptr;
    TokenDropdown* m_toToken = nullptr;
    AmountInput* m_amountInput = nullptr;
    QLabel* m_estimatedOutput = nullptr;
    QLabel* m_refreshCountdown = nullptr;
    QLabel* m_rateLabel = nullptr;
    QLabel* m_routeLabel = nullptr;
    QLabel* m_priceImpactLabel = nullptr;
    QLabel* m_minReceivedLabel = nullptr;
    QLabel* m_slippageLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
    QPushButton* m_swapBtn = nullptr;
    QPushButton* m_flipBtn = nullptr;
    QWidget* m_quoteDetails = nullptr;

    QTimer m_quoteTimer;     // debounce user input -> quote
    QTimer m_refreshTimer;   // auto-refresh quote
    QTimer m_countdownTimer; // 1s tick for countdown display
    int m_countdownSeconds = 0;
    bool m_quoteValid = false;
    bool m_swapInProgress = false;
};

#endif // SWAPPAGE_H
