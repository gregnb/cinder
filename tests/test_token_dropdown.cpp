#include "widgets/TokenDropdown.h"
#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPushButton>
#include <QSignalSpy>
#include <QTest>
#include <QVBoxLayout>
#include <gtest/gtest.h>

static QApplication* app = nullptr;

int main(int argc, char** argv) {
    QApplication a(argc, argv);
    app = &a;
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

class TokenDropdownTest : public ::testing::Test {
  protected:
    // Container simulates being inside a form layout
    QWidget* m_container = nullptr;
    TokenDropdown* m_dropdown = nullptr;

    void SetUp() override {
        m_container = new QWidget();
        m_container->resize(400, 600);

        auto* layout = new QVBoxLayout(m_container);
        m_dropdown = new TokenDropdown();
        layout->addWidget(m_dropdown);

        m_container->show();
        QApplication::processEvents();
    }

    void TearDown() override { delete m_container; }

    QPushButton* button() const { return m_dropdown->findChild<QPushButton*>("tokenDropdownBtn"); }

    QListView* list() const {
        // List is inside the popup which may be reparented
        return m_container->findChild<QListView*>("tokenDropdownList");
    }

    QLineEdit* searchInput() const { return m_container->findChild<QLineEdit*>(); }

    void openDropdown() {
        QTest::mouseClick(button(), Qt::LeftButton);
        QApplication::processEvents();
    }
};

// ── Data Management ──────────────────────────────────────────────

TEST_F(TokenDropdownTest, AddTokenIncreasesItemCount) {
    m_dropdown->addToken(":/icons/tokens/usdc.png", "USDC");
    m_dropdown->addToken(":/icons/tokens/usdt.png", "USDT");
    m_dropdown->addToken(":/icons/tokens/bonk.png", "BONK");

    openDropdown();

    auto* lw = list();
    ASSERT_NE(lw, nullptr);
    EXPECT_EQ(lw->model()->rowCount(), 3);
}

TEST_F(TokenDropdownTest, SetCurrentTokenUpdatesDisplay) {
    m_dropdown->addToken(":/icons/tokens/usdc.png", "USDC");
    m_dropdown->addToken(":/icons/tokens/usdt.png", "USDT");

    m_dropdown->setCurrentToken(":/icons/tokens/usdt.png", "USDT");

    EXPECT_EQ(m_dropdown->currentText(), "USDT");
    EXPECT_EQ(m_dropdown->currentIconPath(), ":/icons/tokens/usdt.png");
}

TEST_F(TokenDropdownTest, CurrentTextReturnsEmptyInitially) {
    EXPECT_EQ(m_dropdown->currentText(), "");
    EXPECT_EQ(m_dropdown->currentIconPath(), "");
}

TEST_F(TokenDropdownTest, AddTokenStoresIconPathInUserRole) {
    m_dropdown->addToken(":/icons/tokens/bonk.png", "BONK");

    openDropdown();

    auto* lw = list();
    ASSERT_NE(lw, nullptr);
    ASSERT_EQ(lw->model()->rowCount(), 1);
    EXPECT_EQ(lw->model()->index(0, 0).data(Qt::UserRole).toString(), ":/icons/tokens/bonk.png");
    EXPECT_EQ(lw->model()->index(0, 0).data(Qt::DisplayRole).toString(), "BONK");
}

// ── Button & Widget Structure ────────────────────────────────────

TEST_F(TokenDropdownTest, ButtonExists) {
    auto* btn = button();
    ASSERT_NE(btn, nullptr);
    EXPECT_EQ(btn->objectName(), "tokenDropdownBtn");
}

TEST_F(TokenDropdownTest, ButtonHasPointingHandCursor) {
    auto* btn = button();
    ASSERT_NE(btn, nullptr);
    EXPECT_EQ(btn->cursor().shape(), Qt::PointingHandCursor);
}

TEST_F(TokenDropdownTest, ListHasPointingHandCursor) {
    m_dropdown->addToken(":/icons/tokens/usdc.png", "USDC");

    openDropdown();

    auto* lw = list();
    ASSERT_NE(lw, nullptr);
    EXPECT_EQ(lw->cursor().shape(), Qt::PointingHandCursor);
}

// ── Dropdown Visibility ──────────────────────────────────────────

TEST_F(TokenDropdownTest, DropdownHiddenInitially) {
    m_dropdown->addToken(":/icons/tokens/usdc.png", "USDC");

    auto* lw = m_container->findChild<QListView*>("tokenDropdownList");
    if (lw) {
        EXPECT_FALSE(lw->isVisible());
    }
}

TEST_F(TokenDropdownTest, ClickButtonOpensDropdown) {
    m_dropdown->addToken(":/icons/tokens/usdc.png", "USDC");
    m_dropdown->addToken(":/icons/tokens/usdt.png", "USDT");

    openDropdown();

    auto* lw = list();
    ASSERT_NE(lw, nullptr);
    EXPECT_TRUE(lw->isVisible());
}

TEST_F(TokenDropdownTest, ClickButtonTogglesDropdown) {
    m_dropdown->addToken(":/icons/tokens/usdc.png", "USDC");

    auto* btn = button();
    ASSERT_NE(btn, nullptr);

    // Open
    QTest::mouseClick(btn, Qt::LeftButton);
    QApplication::processEvents();
    auto* lw = list();
    ASSERT_NE(lw, nullptr);
    EXPECT_TRUE(lw->isVisible());

    // Close
    QTest::mouseClick(btn, Qt::LeftButton);
    QApplication::processEvents();
    EXPECT_FALSE(lw->isVisible());
}

// ── Item Selection ───────────────────────────────────────────────

TEST_F(TokenDropdownTest, ClickItemEmitsSignalAndUpdatesDisplay) {
    m_dropdown->addToken(":/icons/tokens/usdc.png", "USDC");
    m_dropdown->addToken(":/icons/tokens/usdt.png", "USDT");
    m_dropdown->setCurrentToken(":/icons/tokens/usdc.png", "USDC");

    QSignalSpy spy(m_dropdown, &TokenDropdown::tokenSelected);

    openDropdown();

    auto* lw = list();
    ASSERT_NE(lw, nullptr);
    ASSERT_GE(lw->model()->rowCount(), 2);

    // Simulate clicking the second item (USDT)
    QRect itemRect = lw->visualRect(lw->model()->index(1, 0));
    QTest::mouseClick(lw->viewport(), Qt::LeftButton, Qt::NoModifier, itemRect.center());
    QApplication::processEvents();

    // Signal should have been emitted
    ASSERT_EQ(spy.count(), 1);
    QList<QVariant> args = spy.takeFirst();
    EXPECT_EQ(args.at(0).toString(), ":/icons/tokens/usdt.png");
    EXPECT_EQ(args.at(1).toString(), "USDT");

    // Display should update
    EXPECT_EQ(m_dropdown->currentText(), "USDT");
    EXPECT_EQ(m_dropdown->currentIconPath(), ":/icons/tokens/usdt.png");

    // Dropdown should close
    EXPECT_FALSE(lw->isVisible());
}

TEST_F(TokenDropdownTest, ClickItemClearsSelection) {
    m_dropdown->addToken(":/icons/tokens/usdc.png", "USDC");
    m_dropdown->addToken(":/icons/tokens/usdt.png", "USDT");

    openDropdown();

    auto* lw = list();
    ASSERT_NE(lw, nullptr);

    // Click first item
    QRect itemRect = lw->visualRect(lw->model()->index(0, 0));
    QTest::mouseClick(lw->viewport(), Qt::LeftButton, Qt::NoModifier, itemRect.center());
    QApplication::processEvents();

    // No selection should remain (dropdown closes, reopening resets)
    openDropdown();
    lw = list();
    ASSERT_NE(lw, nullptr);
    EXPECT_TRUE(lw->selectionModel()->selectedIndexes().isEmpty());
}

// ── Search ───────────────────────────────────────────────────────

TEST_F(TokenDropdownTest, SearchFiltersTokens) {
    m_dropdown->addToken(":/icons/tokens/usdc.png", "USDC  \xe2\x80\x94  USD Coin");
    m_dropdown->addToken(":/icons/tokens/usdt.png", "USDT  \xe2\x80\x94  Tether");
    m_dropdown->addToken(":/icons/tokens/bonk.png", "BONK  \xe2\x80\x94  Bonk");

    openDropdown();

    auto* search = searchInput();
    ASSERT_NE(search, nullptr);

    // Type "USD" — should match USDC and USDT but not BONK
    QTest::keyClicks(search, "USD");
    QApplication::processEvents();

    auto* lw = list();
    ASSERT_NE(lw, nullptr);
    EXPECT_EQ(lw->model()->rowCount(), 2);
}

TEST_F(TokenDropdownTest, EmptySearchShowsAll) {
    m_dropdown->addToken(":/icons/tokens/usdc.png", "USDC");
    m_dropdown->addToken(":/icons/tokens/usdt.png", "USDT");
    m_dropdown->addToken(":/icons/tokens/bonk.png", "BONK");

    openDropdown();

    auto* search = searchInput();
    ASSERT_NE(search, nullptr);

    // Filter down
    QTest::keyClicks(search, "USD");
    QApplication::processEvents();
    EXPECT_EQ(list()->model()->rowCount(), 2);

    // Clear search — all should return
    search->clear();
    QApplication::processEvents();
    EXPECT_EQ(list()->model()->rowCount(), 3);
}

TEST_F(TokenDropdownTest, SearchClearedOnClose) {
    m_dropdown->addToken(":/icons/tokens/usdc.png", "USDC");
    m_dropdown->addToken(":/icons/tokens/bonk.png", "BONK");

    openDropdown();

    auto* search = searchInput();
    ASSERT_NE(search, nullptr);
    QTest::keyClicks(search, "BONK");
    QApplication::processEvents();
    EXPECT_EQ(list()->model()->rowCount(), 1);

    // Close dropdown
    QTest::mouseClick(button(), Qt::LeftButton);
    QApplication::processEvents();

    // Reopen — search should be cleared, all tokens visible
    openDropdown();
    EXPECT_EQ(searchInput()->text(), "");
    EXPECT_EQ(list()->model()->rowCount(), 2);
}

TEST_F(TokenDropdownTest, SearchIsCaseInsensitive) {
    m_dropdown->addToken(":/icons/tokens/usdc.png", "USDC  \xe2\x80\x94  USD Coin");
    m_dropdown->addToken(":/icons/tokens/bonk.png", "BONK  \xe2\x80\x94  Bonk");

    openDropdown();

    auto* search = searchInput();
    QTest::keyClicks(search, "usdc");
    QApplication::processEvents();

    EXPECT_EQ(list()->model()->rowCount(), 1);
    EXPECT_EQ(list()->model()->index(0, 0).data(Qt::DisplayRole).toString(),
              "USDC  \xe2\x80\x94  USD Coin");
}
