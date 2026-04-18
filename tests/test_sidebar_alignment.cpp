// Regression test for sidebar icon alignment.
//
// The sidebar uses a fixed-width inner widget (280px) that gets clipped when
// collapsed (80px).  Logo and icons must be centered on the same vertical axis
// and must NOT shift positions when toggling between expanded and collapsed.
//
// Constants below must match CinderWalletApp.cpp — see comments for mapping.

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <gtest/gtest.h>

static QApplication* app = nullptr;

int main(int argc, char** argv) {
    QApplication a(argc, argv);
    app = &a;
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

// ── Constants — must stay in sync with CinderWalletApp.cpp ────────────
// Source locations are noted so grep catches drift during code review.

static const int SIDEBAR_EXPANDED = 280; // CinderWalletApp.cpp top-level constant
static const int SIDEBAR_COLLAPSED = 80; // CinderWalletApp.cpp top-level constant

// sidebarLayout->setContentsMargins(18, 20, 18, 28) in createSidebar()
static const int SIDEBAR_MARGIN_LEFT = 18;

// m_logo->setStyleSheet("...margin-left: 2px;") in createSidebar()
static const int LOGO_QSS_MARGIN_LEFT = 2;

// lay->setContentsMargins(6, 0, 6, 0) in createNavButton()
static const int BUTTON_PADDING_LEFT = 6;

static const int LOGO_SIZE = 40; // m_logo->setFixedSize(40, 40)
static const int ICON_SIZE = 32; // iconLabel->setFixedSize(32, 32)

// ── Test fixture ──────────────────────────────────────────────────────

class SidebarAlignmentTest : public ::testing::Test {
  protected:
    QWidget* m_sidebar = nullptr;
    QWidget* m_sidebarInner = nullptr;
    QLabel* m_logo = nullptr;
    QList<QPushButton*> m_navButtons;
    QList<QLabel*> m_navIcons;

    void SetUp() override {
        // Replicate sidebar structure from CinderWalletApp::createSidebar
        m_sidebar = new QWidget();
        m_sidebar->setFixedWidth(SIDEBAR_EXPANDED);
        m_sidebar->setFixedHeight(800);

        m_sidebarInner = new QWidget(m_sidebar);
        m_sidebarInner->setFixedWidth(SIDEBAR_EXPANDED);
        m_sidebarInner->setFixedHeight(800);
        m_sidebarInner->move(0, 0);

        QVBoxLayout* layout = new QVBoxLayout(m_sidebarInner);
        layout->setContentsMargins(SIDEBAR_MARGIN_LEFT, 20, SIDEBAR_MARGIN_LEFT, 28);
        layout->setSpacing(6);

        // Logo — same setup as createSidebar()
        m_logo = new QLabel();
        m_logo->setFixedSize(LOGO_SIZE, LOGO_SIZE);
        QHBoxLayout* logoRow = new QHBoxLayout();
        logoRow->setContentsMargins(LOGO_QSS_MARGIN_LEFT, 0, 0, 0);
        logoRow->setSpacing(0);
        logoRow->addWidget(m_logo);
        logoRow->addStretch();
        layout->addLayout(logoRow);
        layout->addSpacing(32);

        // Nav buttons — same setup as createNavButton()
        for (int i = 0; i < 5; ++i) {
            QPushButton* btn = new QPushButton();
            btn->setFixedHeight(44);

            QHBoxLayout* btnLay = new QHBoxLayout(btn);
            btnLay->setContentsMargins(BUTTON_PADDING_LEFT, 0, BUTTON_PADDING_LEFT, 0);
            btnLay->setSpacing(10);

            QLabel* icon = new QLabel();
            icon->setObjectName("navIcon");
            icon->setFixedSize(ICON_SIZE, ICON_SIZE);
            btnLay->addWidget(icon, 0, Qt::AlignVCenter);

            QLabel* text = new QLabel(QString("Nav %1").arg(i));
            text->setObjectName("navText");
            btnLay->addWidget(text, 1, Qt::AlignVCenter);

            m_navButtons.append(btn);
            m_navIcons.append(icon);
            layout->addWidget(btn);
        }

        layout->addStretch();

        // Force full widget initialization: polish → layout → geometry
        m_sidebar->show();
        m_sidebar->ensurePolished();
        m_sidebarInner->ensurePolished();
        m_logo->ensurePolished();
        for (auto* btn : m_navButtons) {
            btn->ensurePolished();
        }
        QApplication::processEvents();

        // Activate all layouts (outer sidebar → button internals)
        m_sidebarInner->layout()->activate();
        for (auto* btn : m_navButtons) {
            if (btn->layout()) {
                btn->layout()->activate();
            }
        }
        QApplication::processEvents();
    }

    void TearDown() override { delete m_sidebar; }

    int xInSidebar(QWidget* w) const { return w->mapTo(m_sidebarInner, QPoint(0, 0)).x(); }

    int centerXInSidebar(QWidget* w) const { return xInSidebar(w) + w->width() / 2; }
};

// ── Arithmetic sanity check (no Qt needed) ────────────────────────────

TEST(SidebarAlignmentMath, LogoAndIconCentersMatchCollapsedCenter) {
    // Logo center = sidebar_margin + qss_margin + logo_size/2
    int logoCenter = SIDEBAR_MARGIN_LEFT + LOGO_QSS_MARGIN_LEFT + LOGO_SIZE / 2;

    // Icon center = sidebar_margin + button_padding + icon_size/2
    int iconCenter = SIDEBAR_MARGIN_LEFT + BUTTON_PADDING_LEFT + ICON_SIZE / 2;

    // Collapsed sidebar center
    int sidebarCenter = SIDEBAR_COLLAPSED / 2;

    EXPECT_EQ(logoCenter, sidebarCenter)
        << "Logo center (" << logoCenter << ") must equal collapsed sidebar center ("
        << sidebarCenter << ")";

    EXPECT_EQ(iconCenter, sidebarCenter)
        << "Icon center (" << iconCenter << ") must equal collapsed sidebar center ("
        << sidebarCenter << ")";

    EXPECT_EQ(logoCenter, iconCenter) << "Logo and icon centers must match";
}

// ── Layout-based tests ────────────────────────────────────────────────

TEST_F(SidebarAlignmentTest, LogoAndIconsCenteredOnSameAxis) {
    int logoCenter = centerXInSidebar(m_logo);

    for (int i = 0; i < m_navIcons.size(); ++i) {
        int iconCenter = centerXInSidebar(m_navIcons[i]);
        EXPECT_EQ(iconCenter, logoCenter) << "Icon " << i << " center (" << iconCenter
                                          << ") doesn't match logo center (" << logoCenter << ")";
    }
}

TEST_F(SidebarAlignmentTest, AllNavIconsAligned) {
    int firstX = xInSidebar(m_navIcons[0]);

    for (int i = 1; i < m_navIcons.size(); ++i) {
        EXPECT_EQ(xInSidebar(m_navIcons[i]), firstX)
            << "Icon " << i << " x position doesn't match icon 0";
    }
}

TEST_F(SidebarAlignmentTest, CenteredWithinCollapsedWidth) {
    int expected = SIDEBAR_COLLAPSED / 2; // 40

    EXPECT_EQ(centerXInSidebar(m_logo), expected) << "Logo not centered in 80px sidebar";

    for (int i = 0; i < m_navIcons.size(); ++i) {
        EXPECT_EQ(centerXInSidebar(m_navIcons[i]), expected)
            << "Icon " << i << " not centered in 80px sidebar";
    }
}

TEST_F(SidebarAlignmentTest, PositionsStableAcrossCollapseExpand) {
    // Record initial positions
    int logoX = xInSidebar(m_logo);
    QList<int> iconXs;
    for (auto* icon : m_navIcons) {
        iconXs.append(xInSidebar(icon));
    }

    // ── Collapse: shrink outer sidebar, fix button widths ──
    m_sidebar->setFixedWidth(SIDEBAR_COLLAPSED);
    for (auto* btn : m_navButtons) {
        btn->setFixedWidth(44);
    }
    QApplication::processEvents();

    EXPECT_EQ(xInSidebar(m_logo), logoX) << "Logo shifted during collapse";
    for (int i = 0; i < m_navIcons.size(); ++i) {
        EXPECT_EQ(xInSidebar(m_navIcons[i]), iconXs[i])
            << "Icon " << i << " shifted during collapse";
    }

    // ── Expand: restore outer sidebar, clear button constraints ──
    m_sidebar->setFixedWidth(SIDEBAR_EXPANDED);
    for (auto* btn : m_navButtons) {
        btn->setFixedHeight(44);
        btn->setMinimumWidth(0);
        btn->setMaximumWidth(QWIDGETSIZE_MAX);
    }
    m_sidebarInner->layout()->invalidate();
    m_sidebarInner->layout()->activate();
    QApplication::processEvents();

    EXPECT_EQ(xInSidebar(m_logo), logoX) << "Logo shifted during expand";
    for (int i = 0; i < m_navIcons.size(); ++i) {
        EXPECT_EQ(xInSidebar(m_navIcons[i]), iconXs[i]) << "Icon " << i << " shifted during expand";
    }
}
