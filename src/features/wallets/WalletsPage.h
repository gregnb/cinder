#ifndef WALLETSPAGE_H
#define WALLETSPAGE_H

#include "db/WalletDb.h"
#include "features/wallets/WalletsHandler.h"
#include <QWidget>

class ActionIconButton;
class AddressLink;
class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QStackedWidget;
class QTimer;
class QGridLayout;
class QVBoxLayout;
class UploadWidget;

struct WalletGroup {
    WalletSummaryRecord parent;
    QList<WalletSummaryRecord> children;
};

class WalletsPage : public QWidget {
    Q_OBJECT
  public:
    explicit WalletsPage(QWidget* parent = nullptr);

    void refresh();
    void setActiveAddress(const QString& address);

  signals:
    void walletSwitched(const QString& address);
    void walletRemoved(const QString& address);
    void addWalletRequested();
    void addAccountRequested(int parentWalletId);
    void walletRenamed();
    void avatarChanged();

  private:
    enum class Step { List = 0, Detail };
    void showStep(Step step);

    QWidget* buildListView();
    QWidget* buildDetailView();
    void rebuildWalletList();
    QPushButton* buildWalletRow(const WalletSummaryRecord& w, bool isDerived);
    void showWalletDetail(const WalletSummaryRecord& wallet);
    void populateDetail(const WalletRecord& wallet);
    void editWalletName();
    void saveWalletName();
    void saveAvatar(const QString& path);
    void revealPrivateKey();
    void revealRecoveryPhrase();
    void removeWallet();
    void hidePrivateKeyReveal();
    void hideRecoveryPhraseReveal();

    static QList<WalletGroup> groupWallets(const QList<WalletSummaryRecord>& wallets);
    QWidget* makeSeparator();

    QStackedWidget* m_stack = nullptr;
    QString m_activeAddress;

    // List view
    QLabel* m_title = nullptr;
    QVBoxLayout* m_listLayout = nullptr;

    // Detail view
    WalletRecord m_selectedWallet;
    UploadWidget* m_detailAvatar = nullptr;
    QLabel* m_detailName = nullptr;
    QLabel* m_detailType = nullptr;
    QLabel* m_nameValue = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    ActionIconButton* m_nameEditBtn = nullptr;
    QLabel* m_statusPill = nullptr;
    QPushButton* m_switchBtn = nullptr;
    AddressLink* m_addressLink = nullptr;
    QLabel* m_derivPathValue = nullptr;
    QLabel* m_indexValue = nullptr;
    QLabel* m_createdValue = nullptr;
    QWidget* m_derivPathRow = nullptr;
    QWidget* m_indexRow = nullptr;
    QWidget* m_privateKeySection = nullptr;

    // Private key reveal state
    QLabel* m_revealedKeyLabel = nullptr;
    QWidget* m_revealedContainer = nullptr;
    QPushButton* m_revealBtn = nullptr;
    QTimer* m_revealTimer = nullptr;
    QTimer* m_countdownTicker = nullptr;
    QLabel* m_timerLabel = nullptr;
    int m_countdownSecs = 30;

    // Recovery phrase reveal state
    QWidget* m_recoveryCard = nullptr;
    QPushButton* m_recoveryRevealBtn = nullptr;
    QWidget* m_recoveryRevealedContainer = nullptr;
    QGridLayout* m_wordGrid = nullptr;
    QTimer* m_recoveryRevealTimer = nullptr;
    QTimer* m_recoveryCountdownTicker = nullptr;
    QLabel* m_recoveryTimerLabel = nullptr;
    int m_recoveryCountdownSecs = 30;
    WalletsHandler m_handler;
};

#endif // WALLETSPAGE_H
