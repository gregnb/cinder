#ifndef SENDRECEIVEPAGE_H
#define SENDRECEIVEPAGE_H

#include "crypto/Keypair.h"
#include "models/SendReceive.h"
#include "services/model/TransactionResponse.h"
#include <QColor>
#include <QList>
#include <QMap>
#include <QSet>
#include <QWidget>
#include <memory>

class QStackedWidget;
class QScrollArea;
class QLineEdit;
class QPushButton;
class QLabel;
class QVBoxLayout;
class QCheckBox;
class QSpinBox;
class QTextEdit;
class QTimer;
class AddressInput;
class AddressLink;
class AmountInput;
class TokenDropdown;
class StyledCheckbox;
class AvatarCache;
class Signer;
class SolanaApi;
class UploadWidget;
class PillButtonGroup;
class TxStatusAnimationWidget;
class SendReceiveHandler;
class CreateTokenHandler;
class MintTokenHandler;
class BurnTokenHandler;
struct CloseTokenAccountEntry;
class CloseTokenAccountsHandler;

class SendReceivePage : public QWidget {
    Q_OBJECT
  public:
    explicit SendReceivePage(QWidget* parent = nullptr);
    ~SendReceivePage() override;

    void openWithRecipient(const QString& address);
    void openWithToken(const QString& iconPath);
    void openWithMint(const QString& mint);
    void setExternalEntry(bool external);
    void setWalletAddress(const QString& address);
    void refreshBalances();
    void setSolanaApi(SolanaApi* api);
    void setAvatarCache(AvatarCache* cache);
    void setKeypair(const Keypair& kp);
    void setSigner(Signer* signer);
    void debugShowReview();

  signals:
    void backRequested();
    void transactionSent(const QString& signature);

  protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

  private:
    void initializeStatusLabel(QLabel* label);
    void setStatusLabelState(QLabel* label, const QString& text, bool isError);

    enum class StackPage {
        CardGrid = 0,
        SendForm,
        Review,
        CloseAccounts,
        BurnTokens,
        NonceSetup,
        CreateToken,
        MintTokens,
        Success,
    };

    void setCurrentPage(StackPage page);

    struct RecipientRow {
        QWidget* container = nullptr;
        QLabel* label = nullptr;
        AddressInput* addressInput = nullptr;
        AmountInput* amountInput = nullptr;
        QPushButton* deleteBtn = nullptr;
        QWidget* deleteCol = nullptr;
    };

    QStackedWidget* m_stack = nullptr;
    TokenDropdown* m_tokenDropdown = nullptr;
    bool m_externalEntry = false;
    QString m_walletAddress;
    SolanaApi* m_solanaApi = nullptr;
    Keypair m_keypair;
    Signer* m_signer = nullptr;
    QMap<QString, SendReceiveTokenMeta> m_tokenMeta;     // keyed by iconPath (send page)
    QMap<QString, SendReceiveTokenMeta> m_mintTokenMeta; // keyed by iconPath (mint page)
    std::unique_ptr<SendReceiveHandler> m_handler;
    std::unique_ptr<CreateTokenHandler> m_createTokenHandler;
    std::unique_ptr<MintTokenHandler> m_mintTokenHandler;
    std::unique_ptr<BurnTokenHandler> m_burnTokenHandler;
    std::unique_ptr<CloseTokenAccountsHandler> m_closeTokenAccountsHandler;

    QVBoxLayout* m_recipientsLayout = nullptr;
    QList<RecipientRow> m_recipientRows;
    QPushButton* m_addRecipientBtn = nullptr;
    QPushButton* m_reviewBtn = nullptr;
    PillButtonGroup* m_speedSelector = nullptr;
    QWidget* m_customFeeRow = nullptr;
    AmountInput* m_customFeeInput = nullptr;
    QScrollArea* m_reviewScroll = nullptr;
    QScrollArea* m_closeAccountsScroll = nullptr;
    QList<QCheckBox*> m_closeAccountCheckboxes;
    QList<CloseTokenAccountEntry> m_closeAccountEntries;
    QCheckBox* m_selectAllCheckbox = nullptr;
    QPushButton* m_closeAccountsBtn = nullptr;
    QLabel* m_closeAccountsSummary = nullptr;
    QLabel* m_closeAccountsStatusLabel = nullptr;

    // Send review actions
    QPushButton* m_sendConfirmBtn = nullptr;
    QLabel* m_sendStatusLabel = nullptr;
    // Create-token review actions
    QPushButton* m_createTokenConfirmBtn = nullptr;
    QLabel* m_createTokenStatusLabel = nullptr;

    // Transfer fee state (Token-2022)
    quint16 m_transferFeeBasisPoints = 0;
    quint64 m_transferFeeMax = 0;

    // Mint tokens page
    QScrollArea* m_mintScroll = nullptr;
    TokenDropdown* m_mintTokenDropdown = nullptr;
    QLabel* m_mintBalanceLabel = nullptr;
    AmountInput* m_mintAmountInput = nullptr;
    QPushButton* m_mintBtn = nullptr;
    QLabel* m_mintStatusLabel = nullptr;
    QMetaObject::Connection m_mintAuthorityConn;
    QSet<QString> m_mintAuthorityPending;

    // Burn tokens page
    QScrollArea* m_burnScroll = nullptr;
    TokenDropdown* m_burnTokenDropdown = nullptr;
    QLabel* m_burnBalanceLabel = nullptr;
    AmountInput* m_burnAmountInput = nullptr;
    QPushButton* m_burnBtn = nullptr;
    QLabel* m_burnStatusLabel = nullptr;
    QMap<QString, SendReceiveTokenMeta> m_burnTokenMeta; // keyed by iconPath (burn page)

    // Success page
    TxStatusAnimationWidget* m_txStatusAnim = nullptr;
    QLabel* m_successTitle = nullptr;
    QLabel* m_successAmount = nullptr;
    QLabel* m_successNetworkFeeVal = nullptr;
    QWidget* m_successNetworkFeeRow = nullptr;
    QLabel* m_successTxVersionVal = nullptr;
    QLabel* m_successResultVal = nullptr;
    AddressLink* m_successSignerLink = nullptr;
    QWidget* m_successRecipientRow = nullptr;
    AddressLink* m_successRecipientLink = nullptr;
    AddressLink* m_successSignatureLink = nullptr;
    QWidget* m_successMintRow = nullptr;
    AddressLink* m_successMintLink = nullptr;
    QWidget* m_successBlockRow = nullptr;
    QLabel* m_successBlockVal = nullptr;
    QWidget* m_successTimestampRow = nullptr;
    QLabel* m_successTimestampVal = nullptr;
    QWidget* m_successBlockhashRow = nullptr;
    AddressLink* m_successBlockhashLink = nullptr;
    QPushButton* m_successSolscanBtn = nullptr;
    QPushButton* m_successDoneBtn = nullptr;
    QString m_successSignature;

    // Confirmation polling state
    QObject* m_confirmGuard = nullptr;
    QWidget* m_confirmBadge = nullptr;
    QWidget* m_confirmSpinner = nullptr;
    QTimer* m_confirmSpinTimer = nullptr;
    int m_confirmAngle = 0;

    // Durable nonce state
    StyledCheckbox* m_nonceCheckbox = nullptr;
    QWidget* m_nonceInfoRow = nullptr;
    AddressLink* m_nonceAddrLabel = nullptr;
    bool m_nonceEnabled = false;
    QString m_nonceAddress;
    QString m_nonceValue;
    QLabel* m_nonceCostLabel = nullptr;
    QPushButton* m_createNonceBtn = nullptr;
    QLabel* m_nonceStatusLabel = nullptr;
    QLabel* m_nonceAuthorityLabel = nullptr;
    quint64 m_nonceRentLamports = 0;
    QString m_pendingNonceAddress; // set after TX sent, for retry
    QTimer* m_nonceDotsTimer = nullptr;
    int m_nonceDotCount = 0;
    QString m_nonceStatusBase; // base text for dot animation

    void pollNonceAccount(const QString& nonceAddress);
    void startNonceDots(const QString& baseText);
    void stopNonceDots();
    void wireNonceRetryButton();
    void restoreCreateNonceButton();
    void applyReadyNonceState(const QString& nonceAddress, const QString& nonceValue);

    QWidget* buildCardGrid();
    QWidget* buildSendForm();
    QWidget* buildReviewPage();
    void populateReviewPage();
    QWidget* buildCloseAccountsPage();
    void populateCloseAccountsPage();
    void updateCloseAccountsSummary();
    void executeCloseAccounts();
    QWidget* buildMintTokensPage();
    void refreshMintTokens();
    void populateMintDropdownFromDb();
    void fetchMintAuthorities();
    void validateMintForm();
    void executeMint();
    QWidget* buildBurnTokensPage();
    void refreshBurnTokens();
    void populateBurnDropdownFromDb();
    void validateBurnForm();
    void executeBurn();
    void openSendForm(bool preSelectSol);

    void addRecipientRow();
    void removeRecipientRow(int index);
    void updateRecipientLabels();
    void validateForm();
    void resetForm();
    void executeSend();
    void exportReviewPdf();
    void exportReviewCsv();

    QWidget* createActionCard(const QString& iconPath, const QColor& accent, const QString& title,
                              const QString& subtitle, int iconSize);
    void applyCardStyle(QWidget* card, const QColor& accent, double borderOpacity);

    QWidget* createSpeedSelector();

    // Transaction status page
    QWidget* buildSuccessPage();
    void showSuccessPage(const SendReceiveSuccessPageInfo& info);
    void startConfirmationPolling(const QString& signature);
    void updateStatusConfirmed(const TransactionResponse& tx);
    void updateStatusFailed(const TransactionResponse& tx);
    void updateStatusTimeout();
    void startConfirmDots();
    void stopConfirmDots();

    QWidget* buildNonceSetupPage();
    void onNonceCheckboxToggled(bool checked);
    void createNonceAccount();

    // Create Token (Token-2022)
    QWidget* buildCreateTokenPage();
    QWidget* buildCreateTokenIdentityCard(QScrollArea* scroll);
    QWidget* buildCreateTokenSupplyCard();
    QWidget* buildCreateTokenAuthoritiesCard();
    QWidget* buildCreateTokenExtensionsCard(QScrollArea* scroll);
    QPushButton* buildCreateTokenReviewButton();
    SendReceiveCreateTokenBuildInput collectCreateTokenBuildInput() const;
    void updateCreateTokenReviewButtonState();
    void populateCreateTokenReview();
    void executeCreateToken();
    quint64 computeMintAccountSize() const;

    QLineEdit* m_ctName = nullptr;
    QLineEdit* m_ctSymbol = nullptr;
    QTextEdit* m_ctDescription = nullptr;
    QLabel* m_ctDescCounter = nullptr;
    UploadWidget* m_ctImageUpload = nullptr;
    // Arweave upload
    StyledCheckbox* m_ctUploadCheck = nullptr;
    QWidget* m_ctUploadInfo = nullptr;
    QLabel* m_ctUploadCost = nullptr;
    QLabel* m_ctUploadWarn = nullptr;
    quint64 m_ctUploadLamports = 0;
    // Metadata URI (refs for toggling visibility)
    QLabel* m_ctUriLabel = nullptr;
    QLineEdit* m_ctUri = nullptr;
    QLabel* m_ctUriHint = nullptr;
    QSpinBox* m_ctDecimals = nullptr;
    QLineEdit* m_ctSupply = nullptr;
    StyledCheckbox* m_ctFreezeAuth = nullptr;
    // Extension toggles
    StyledCheckbox* m_ctTransferFee = nullptr;
    QSpinBox* m_ctFeeBps = nullptr;
    QLineEdit* m_ctFeeMax = nullptr;
    QWidget* m_ctFeeDetails = nullptr;
    StyledCheckbox* m_ctNonTransferable = nullptr;
    StyledCheckbox* m_ctMintClose = nullptr;
    StyledCheckbox* m_ctPermDelegate = nullptr;
    QPushButton* m_ctReviewBtn = nullptr;
    // State
    Keypair m_mintKeypair;
    bool m_isCreateTokenReview = false;
};

#endif // SENDRECEIVEPAGE_H
