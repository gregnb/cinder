#ifndef ASSETSPAGE_H
#define ASSETSPAGE_H

#include "models/Assets.h"
#include <QHash>
#include <QList>
#include <QWidget>

class AddressLink;
class Dropdown;
class QLabel;
class QLineEdit;
class QScrollArea;
class QStackedWidget;
class SplineChart;
class AvatarCache;
class QrCodeWidget;
class AssetsHandler;

class AssetsPage : public QWidget {
    Q_OBJECT
  public:
    explicit AssetsPage(QWidget* parent = nullptr);

    void setAvatarCache(AvatarCache* cache);

    // Reload assets from the database for the given wallet address.
    void refresh(const QString& ownerAddress);

  signals:
    void sendAsset(const QString& mint);

  private:
    enum class Step { AssetList = 0, Receive };
    void showStep(Step step);

    QWidget* createPortfolioCard();
    QWidget* createPoolCard();
    void bindCard(QWidget* card, int filteredIndex);
    QWidget* buildReceivePage();
    void relayoutVisibleCards();
    void rebuildFilteredList();
    void filterAssets(const QString& text);
    void showReceivePage();
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

    // Virtual grid constants
    static constexpr int CARD_H = 230;
    static constexpr int GRID_COLS = 3;
    static constexpr int GRID_SPACING = 20;
    static constexpr int BUFFER_ROWS = 2;
    static constexpr int POOL_SIZE = 30;

    QList<AssetInfo> m_assets;
    QList<int> m_filteredIndices;
    QString m_currentFilter;
    QList<QWidget*> m_cardPool;
    QHash<int, QWidget*> m_activeCards;
    QScrollArea* m_scroll = nullptr;
    QWidget* m_gridContainer = nullptr;
    QStackedWidget* m_stack = nullptr;
    QLabel* m_title = nullptr;
    QLabel* m_portfolioAmount = nullptr;
    SplineChart* m_chart = nullptr;
    Dropdown* m_sortDropdown = nullptr;
    QLineEdit* m_searchInput = nullptr;
    QWidget* m_emptyState = nullptr;
    QrCodeWidget* m_qrCode = nullptr;
    AddressLink* m_receiveAddress = nullptr;
    QString m_ownerAddress;
    AvatarCache* m_avatarCache = nullptr;
    AssetsHandler* m_handler = nullptr;
};

#endif // ASSETSPAGE_H
