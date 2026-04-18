#ifndef TOKENDROPDOWN_H
#define TOKENDROPDOWN_H

#include <QWidget>

class QAbstractItemModel;
class QSortFilterProxyModel;
class QPushButton;
class QListView;
class QLineEdit;
class QLabel;
class AvatarCache;

class TokenDropdown : public QWidget {
    Q_OBJECT
  public:
    enum SortOrder { NoSort, AlphabeticalAsc, AlphabeticalDesc };

    explicit TokenDropdown(QWidget* parent = nullptr);
    ~TokenDropdown() override;

    void addToken(const QString& iconPath, const QString& displayName,
                  const QString& balance = "0.00");
    void setCurrentToken(const QString& iconPath, const QString& displayName,
                         const QString& balance = "0.00");
    void setAvatarCache(AvatarCache* cache);
    void sortItems(SortOrder order = AlphabeticalAsc);
    void clear();
    bool selectByIcon(const QString& iconPath);
    QString currentText() const;
    QString currentIconPath() const;
    QString currentBalanceText() const;

  signals:
    void tokenSelected(const QString& iconPath, const QString& displayName);

  protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

  private:
    void showDropdown();
    void hideDropdown();
    void recalcPopupSize();
    void updateChevron(bool open);

    // Button
    QPushButton* m_button = nullptr;
    QLabel* m_icon = nullptr;
    QLabel* m_name = nullptr;
    QLabel* m_balance = nullptr;
    QLabel* m_chevron = nullptr;
    QString m_currentIconPath;

    // Popup
    QWidget* m_popup = nullptr;
    QLineEdit* m_searchInput = nullptr;
    QListView* m_listView = nullptr;
    QAbstractItemModel* m_model = nullptr;
    QSortFilterProxyModel* m_proxy = nullptr;
    QLabel* m_emptyLabel = nullptr;
    AvatarCache* m_avatarCache = nullptr;
};

#endif // TOKENDROPDOWN_H
