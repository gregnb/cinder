#ifndef STAKINGPAGE_H
#define STAKINGPAGE_H

#include "crypto/Keypair.h"
#include "models/DashboardStakeItem.h"
#include "models/Staking.h"
#include "services/model/SignatureInfo.h"
#include "services/model/StakeAccountInfo.h"
#include "services/model/StakeRewardInfo.h"
#include "services/model/TransactionResponse.h"
#include "services/model/ValidatorInfo.h"
#include <QHash>
#include <QMap>
#include <QSet>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QStackedWidget;
class QVBoxLayout;
class AddressLink;
class AvatarCache;
class PillButtonGroup;
class Signer;
class SolanaApi;
class StakeStatusRail;
class ValidatorService;
class TabBar;
class StakingHandler;
class QTimer;

class StakingPage : public QWidget {
    Q_OBJECT
  public:
    explicit StakingPage(QWidget* parent = nullptr);

    void setSolanaApi(SolanaApi* api);
    void setKeypair(const Keypair& kp);
    void setSigner(Signer* signer);
    void setWalletAddress(const QString& address);
    void refresh();
    void prefetchValidators();
    void setAvatarCache(AvatarCache* cache);
    void setAutoRefreshOnShow(bool enabled);
    void openStakeDetail(const QString& stakeAddress);

  signals:
    void stakingSummaryChanged(const QList<DashboardStakeItem>& items);
    void transactionClicked(const QString& signature);

  protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
    void showEvent(QShowEvent* event) override;

  private:
    enum class Step { Main = 0, StakeForm, StakeDetail };
    void showStep(Step step);

    // Build methods
    QWidget* buildValidatorBrowser();
    QWidget* buildMyStakes();
    QWidget* buildStakeForm();
    QWidget* buildStakeDetail();

    // Sort
    enum class SortColumn { Name, Stake, Apy, Commission, Version };
    void sortValidators(SortColumn col);
    void updateHeaderLabels();
    void updateSortIcon(QPushButton* btn, int state);

    // Virtual validator list
    QWidget* createPoolRow();
    void bindRow(QWidget* row, int dataIndex);
    void relayoutVisibleRows();
    void rebuildFilteredList(const QString& filter = {});

    void populateMyStakes();
    void populateStakeDetail();
    void preloadStakeDetailData();
    void resetStakeDetailActivityViews();
    void loadStakeDetailTransactions();
    void renderStakeDetailTransactions();
    void renderStakeDetailRewards();
    void refreshStakeDetailRewardTotal();
    void requestStakeDetailRewards();

    // Actions
    void showStakeDialog(const ValidatorInfo& validator);
    void doStake(const QString& voteAccount, quint64 lamports);
    void doDeactivate(const QString& stakeAccount);
    void doWithdraw(const QString& stakeAccount, quint64 lamports);

    void updateAvatarForUrl(const QString& url);

    // State
    AvatarCache* m_avatarCache = nullptr;
    SolanaApi* m_solanaApi = nullptr;
    ValidatorService* m_validatorService = nullptr;
    StakingHandler* m_stakingHandler = nullptr;
    Keypair m_keypair;
    Signer* m_signer = nullptr;
    QString m_walletAddress;
    quint64 m_currentEpoch = 0;
    quint64 m_solBalance = 0;
    quint64 m_rentExempt = 0;
    QList<ValidatorInfo> m_validators;
    QList<StakeAccountInfo> m_stakeAccounts;
    QHash<QString, StakeAccountInfo> m_pendingStakeAccounts;

    // Currently selected validator for stake form
    ValidatorInfo m_selectedValidator;

    // UI
    QStackedWidget* m_stack = nullptr;
    TabBar* m_tabs = nullptr;
    QWidget* m_validatorPage = nullptr;
    QWidget* m_myStakesPage = nullptr;
    QWidget* m_stakeFormPage = nullptr;
    QLineEdit* m_searchInput = nullptr;
    QVBoxLayout* m_myStakesLayout = nullptr;

    // Virtual validator list
    QScrollArea* m_validatorScroll = nullptr;
    QWidget* m_validatorContainer = nullptr;
    QList<QWidget*> m_rowPool;
    QList<int> m_filteredValidators;
    QHash<int, QWidget*> m_activeRows; // dataIndex → bound pool row
    QLabel* m_noMatchLabel = nullptr;
    static constexpr int ROW_H = 58;
    static constexpr int BUFFER_ROWS = 20;
    static constexpr int POOL_SIZE = 80;
    QLabel* m_summaryLabel = nullptr;
    QLabel* m_stakeSummaryLabel = nullptr;
    QLabel* m_statusLabel = nullptr;

    // Stake form widgets
    QLabel* m_formValidatorName = nullptr;
    QLabel* m_formIcon = nullptr;
    QLabel* m_formApy = nullptr;
    QLabel* m_formCommission = nullptr;
    AddressLink* m_formVoteAccount = nullptr;
    AddressLink* m_formNodePubkey = nullptr;
    QLabel* m_formLocation = nullptr;
    QLabel* m_formUptime = nullptr;
    QLabel* m_formStake = nullptr;
    QLabel* m_formVersion = nullptr;
    QLabel* m_formScore = nullptr;
    QLineEdit* m_amountInput = nullptr;
    QPushButton* m_stakeBtn = nullptr;
    QLabel* m_formStatus = nullptr;

    // Stake detail widgets
    QWidget* m_stakeDetailPage = nullptr;
    QLabel* m_detailTitle = nullptr;
    QLabel* m_detailAddress = nullptr;
    StakeStatusRail* m_detailLifecycle = nullptr;
    QLabel* m_detailLockup = nullptr;
    QLabel* m_detailTotalValue = nullptr;
    QLabel* m_detailType = nullptr;
    QLabel* m_detailState = nullptr;
    QLabel* m_detailDelegatedStake = nullptr;
    QLabel* m_detailActiveStake = nullptr;
    QLabel* m_detailInactiveStake = nullptr;
    QLabel* m_detailTotalRewards = nullptr;
    QLabel* m_detailRentReserve = nullptr;
    QLabel* m_detailValidator = nullptr;
    AddressLink* m_detailVoteAccount = nullptr;
    AddressLink* m_detailWithdrawAuthority = nullptr;
    AddressLink* m_detailStakeAuthority = nullptr;
    QLabel* m_detailActivationEpoch = nullptr;
    QLabel* m_detailDeactivationEpoch = nullptr;
    QLabel* m_detailAllocatedSize = nullptr;
    QLabel* m_detailCustodian = nullptr;
    QPushButton* m_detailPrimaryAction = nullptr;
    PillButtonGroup* m_detailActivityTabs = nullptr;
    QStackedWidget* m_detailActivityStack = nullptr;
    QLabel* m_detailTransactionsStatus = nullptr;
    QVBoxLayout* m_detailTransactionsLayout = nullptr;
    QLabel* m_detailRewardsStatus = nullptr;
    QVBoxLayout* m_detailRewardsLayout = nullptr;
    StakeAccountInfo m_selectedStakeAccount;
    QList<SignatureInfo> m_detailSignatures;
    QHash<QString, TransactionResponse> m_detailTransactions;
    QSet<QString> m_detailFailedTransactions;
    QMap<quint64, StakeRewardInfo> m_detailEpochRewards;
    QSet<quint64> m_detailPendingRewardEpochs;
    qint64 m_epochEtaTargetSecs = 0;
    double m_avgSecondsPerSlot = 0.4;
    quint64 m_epochSlotIndex = 0;
    quint64 m_slotsInEpoch = 0;
    QTimer* m_detailEtaTimer = nullptr;
    bool m_detailRewardEstimateValid = false;
    QString m_detailRewardEstimateAddress;
    QString m_detailRewardEstimateVoteAccount;
    quint64 m_detailRewardEstimateEpoch = 0;
    quint64 m_detailRewardEstimateActiveLamports = 0;
    double m_detailRewardEstimateSol = 0.0;

    // Sort state
    SortColumn m_sortColumn = SortColumn::Stake;
    bool m_sortAscending = false; // default: descending (highest first)
    QList<QPushButton*> m_sortBtns;
    QList<QPushButton*> m_headerLabels;

    bool m_hasRefreshed = false;
    bool m_autoRefreshOnShow = true;
    bool m_stakeAccountsConnected = false;

    void updateStakeDetailEta();
    void invalidateStakeDetailRewardEstimate();
};

#endif // STAKINGPAGE_H
