#ifndef ACCOUNTSELECTOR_H
#define ACCOUNTSELECTOR_H

#include "db/WalletDb.h"
#include <QList>
#include <QTimer>
#include <QWidget>

class QLabel;
class QPushButton;
class QScrollArea;
class QVBoxLayout;

class AccountSelector : public QWidget {
    Q_OBJECT
  public:
    explicit AccountSelector(QWidget* parent = nullptr);
    ~AccountSelector() override;

    void setAccounts(const QList<WalletSummaryRecord>& wallets);
    void setActiveAddress(const QString& address);
    void setCollapsed(bool collapsed);

  signals:
    void accountSwitched(const QString& address);
    void addAccountRequested(int parentWalletId);
    void addWalletRequested();

  protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

  private:
    void showPopup();
    void hidePopup();
    void rebuildPopup();
    QPushButton* buildPopupRow(const WalletSummaryRecord& wallet);
    void updateButton();
    void updateChevron(bool open);
    static QColor avatarColor(const QString& address);
    static QString truncateAddress(const QString& address);

    QPushButton* m_button = nullptr;
    QLabel* m_avatar = nullptr;
    QLabel* m_addressLabel = nullptr;
    QLabel* m_copyIcon = nullptr;
    QTimer m_copyFeedbackTimer;
    QLabel* m_chevron = nullptr;

    QWidget* m_popup = nullptr;
    QVBoxLayout* m_popupLayout = nullptr;
    QScrollArea* m_scrollArea = nullptr;
    QWidget* m_scrollContent = nullptr;
    QVBoxLayout* m_scrollLayout = nullptr;

    QList<WalletSummaryRecord> m_accounts;
    QString m_activeAddress;
    bool m_collapsed = false;
};

#endif // ACCOUNTSELECTOR_H
