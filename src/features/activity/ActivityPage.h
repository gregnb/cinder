#ifndef ACTIVITYPAGE_H
#define ACTIVITYPAGE_H

#include "features/activity/ActivityHandler.h"
#include <QSet>
#include <QTimer>
#include <QWidget>

class QHBoxLayout;
class QLabel;
class QPushButton;
class QVBoxLayout;
class Dropdown;
class AvatarCache;

class ActivityPage : public QWidget {
    Q_OBJECT
  public:
    explicit ActivityPage(QWidget* parent = nullptr);

    void setAvatarCache(AvatarCache* cache);
    void refresh(const QString& ownerAddress);
    void refreshKeepingFilters();
    void setBackfillRunning(bool running);
    int totalRows() const { return m_unfilteredTotalRows; }

  signals:
    void transactionClicked(const QString& signature);

  protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

  private:
    void populateRows(const QList<ActivityRowView>& rows);
    void applyAllFilters();
    void rebuildChips();
    void clearAllFilters();
    void refreshRelativeTimestamps();
    void updateFilterBtnStyle(QPushButton* btn, bool active);
    void updateSortIcon(QPushButton* btn, int state);
    void toggleSort(int column);
    void applySortToRows();
    void loadPage();
    ActivityFilters currentFilters() const;

    // Filter popup helpers
    void showTextFilter(QPushButton* anchor, const QString& placeholder, QString& state);
    void showTimeFilter(QPushButton* anchor);
    void showActionFilter(QPushButton* anchor);
    void showAmountFilter(QPushButton* anchor);

    // UI
    QVBoxLayout* m_listLayout = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
    QWidget* m_syncBadge = nullptr;
    QWidget* m_syncSpinner = nullptr;
    QLabel* m_syncText = nullptr;
    QTimer m_syncSpinTimer;
    int m_syncAngle = 0;
    QWidget* m_headerRow = nullptr;
    QWidget* m_chipBar = nullptr;
    QHBoxLayout* m_chipLayout = nullptr;
    QString m_ownerAddress;

    // Pagination
    Dropdown* m_pageSizeDropdown = nullptr;
    QPushButton* m_firstPageBtn = nullptr;
    QPushButton* m_prevPageBtn = nullptr;
    QPushButton* m_nextPageBtn = nullptr;
    QPushButton* m_lastPageBtn = nullptr;
    QLabel* m_showingLabel = nullptr;
    int m_pageSize = 100;
    int m_currentPage = 0;
    int m_totalRows = 0;           // filtered count (used for pagination)
    int m_unfilteredTotalRows = 0; // total without filters (for "X of Y" display)

    // Filter icon buttons (for active indicator styling)
    QPushButton* m_sigFilterBtn = nullptr;
    QPushButton* m_timeFilterBtn = nullptr;
    QPushButton* m_actionFilterBtn = nullptr;
    QPushButton* m_fromFilterBtn = nullptr;
    QPushButton* m_toFilterBtn = nullptr;
    QPushButton* m_amountFilterBtn = nullptr;
    QPushButton* m_tokenFilterBtn = nullptr;

    // Sort buttons, labels, and state (0=none, 1=asc, 2=desc)
    QList<QPushButton*> m_sortBtns;
    QList<QLabel*> m_sortLabels;
    int m_sortColumn = -1;
    int m_sortDirection = 0; // 0=none, 1=asc, 2=desc

    // Filter state
    QString m_sigFilter;
    qint64 m_timeFrom = 0;
    qint64 m_timeTo = 0;
    QSet<QString> m_actionFilter;
    QString m_fromFilter;
    QString m_toFilter;
    double m_amountMin = -1;
    double m_amountMax = -1;
    QString m_tokenFilter;

    AvatarCache* m_avatarCache = nullptr;
    ActivityHandler m_handler;

    // Row data
    QTimer m_relativeTimeTimer;
    struct ActivityRow {
        QWidget* rowWidget;
        QLabel* timeLabel;
        qint64 blockTime;
        QString signature;
        QString activityType;
        QString fromAddress;
        QString toAddress;
        double amount;
        QString tokenSymbol;
    };
    QList<ActivityRow> m_activityRows;
};

#endif // ACTIVITYPAGE_H
