#ifndef TXLOOKUPPAGE_H
#define TXLOOKUPPAGE_H

#include <QList>
#include <QMap>
#include <QMetaObject>
#include <QWidget>

class QStackedWidget;
class QLineEdit;
class QPushButton;
class QLabel;
class QVBoxLayout;
class PillButtonGroup;
class SolanaApi;
class IdlRegistry;
class AvatarCache;
class TxLookupHandler;
struct TransactionResponse;

class ComputeUnitsBar;

class TxLookupPage : public QWidget {
    Q_OBJECT
  public:
    explicit TxLookupPage(SolanaApi* api, IdlRegistry* idlRegistry = nullptr,
                          QWidget* parent = nullptr);

    void setAvatarCache(AvatarCache* cache);
    void setWalletAddress(const QString& address);

    // Navigate directly to detail page with a given transaction signature
    void openWithSignature(const QString& signature);

  signals:
    void backRequested();

  private:
    enum class Step { Input = 0, Detail };
    void showStep(Step step);

    IdlRegistry* m_idlRegistry = nullptr;
    AvatarCache* m_avatarCache = nullptr;
    QString m_walletAddress;
    bool m_externalEntry = false;
    QMetaObject::Connection m_dexNameConn;
    TxLookupHandler* m_handler = nullptr;

    QStackedWidget* m_stack = nullptr;
    QLineEdit* m_txInput = nullptr;
    QPushButton* m_analyzeBtn = nullptr;

    // Detail page internals
    QStackedWidget* m_detailTabs = nullptr;
    PillButtonGroup* m_tabGroup = nullptr;

    // Summary card
    QLabel* m_summaryIcon = nullptr;
    QVBoxLayout* m_summaryLayout = nullptr;

    // Overview detail labels
    QLabel* m_sigLabel = nullptr;
    QLabel* m_blockLabel = nullptr;
    QLabel* m_timestampLabel = nullptr;
    QLabel* m_resultBadge = nullptr;
    QLabel* m_confirmLabel = nullptr;
    QLabel* m_signerLabel = nullptr;
    QLabel* m_feeLabel = nullptr;
    QLabel* m_priorityFeeLabel = nullptr;
    QLabel* m_cuConsumedLabel = nullptr;
    QLabel* m_versionLabel = nullptr;
    QLabel* m_blockHashLabel = nullptr;
    QWidget* m_nonceRow = nullptr;
    QLabel* m_nonceLabel = nullptr;

    // Compute units section
    ComputeUnitsBar* m_cuBar = nullptr;
    QLabel* m_cuTotalLabel = nullptr;
    QVBoxLayout* m_cuLegendLayout = nullptr;

    // Dynamic tab layouts (cleared & rebuilt on each transaction)
    QVBoxLayout* m_instrLayout = nullptr;
    QVBoxLayout* m_logsLayout = nullptr;
    QVBoxLayout* m_balancesLayout = nullptr;

    QWidget* buildInputPage();
    QWidget* buildDetailPage();
    QWidget* buildOverviewTab();
    QWidget* buildInstructionsTab();
    QWidget* buildBalancesTab();
    QWidget* buildLogsTab();
    void validateInput();
    void loadTransaction(const QString& signature);
    void resetAnalyzeButton();
    void handleFetchedTransaction(const QString& sig, const TransactionResponse& tx);
    void populateOverview(const QString& signature, const TransactionResponse& tx);
    void populateInstructions(const TransactionResponse& tx);
    void populateComputeUnits(const TransactionResponse& tx);
    void populateBalances(const TransactionResponse& tx);
    void populateLogs(const TransactionResponse& tx);

    // Address hover highlighting
    QMap<QString, QList<QLabel*>> m_addressLabels;
    QString m_hoveredAddress;
    void registerAddressLabel(const QString& address, QLabel* label);
    void highlightAddress(const QString& address);
    void clearHighlight();
    bool eventFilter(QObject* obj, QEvent* event) override;

    // Custom tooltip (native macOS tooltips ignore Qt styling)
    QLabel* m_customTooltip = nullptr;
    void showCustomTooltip(const QPoint& globalPos, const QString& text);
    void hideCustomTooltip();
};

#endif // TXLOOKUPPAGE_H
