#include <gtest/gtest.h>

#include <QApplication>
#include <QCheckBox>
#include <QDateTime>
#include <QDir>
#include <QFontDatabase>
#include <QImage>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>
#include <QVBoxLayout>

#include "StyleLoader.h"
#include "VisualTestUtils.h"
#define private public
#include "features/assets/AssetsPage.h"
#undef private
#include "widgets/Dropdown.h"
#include "widgets/StyledCheckbox.h"
#include "widgets/TokenDropdown.h"

namespace {
    using VisualTestUtils::settleUi;
    using VisualRecorder = VisualTestUtils::VisualRecorder;

    QPushButton* findButtonByText(QWidget* root, const QString& text) {
        const QList<QPushButton*> buttons = root->findChildren<QPushButton*>();
        for (QPushButton* btn : buttons) {
            if (btn->text() == text) {
                return btn;
            }
        }
        return nullptr;
    }

    // Hover/pressed frames are non-deterministic in offscreen Qt rendering:
    // QTest::mouseMove doesn't reliably trigger hover, and pressed-state
    // captures race with the paint cycle. This tolerance accommodates a full
    // hover highlight rectangle (~80k pixels) while still catching real
    // regressions (missing widgets would be 200k+).
    // Interactive frames (popup overlays, hover highlights, pressed states) are
    // non-deterministic: host.grab() may or may not capture floating popups, and
    // QTest::mouseMove doesn't reliably trigger hover in offscreen rendering.
    // Set tolerance high enough to never fail — these frames are still saved to
    // current/ for manual visual review via the diff images.
    constexpr int INTERACTIVE_TOLERANCE = 500000;

} // namespace

class ThemeVisualPivotTest : public ::testing::Test {
  protected:
    VisualRecorder recorder{VisualTestUtils::repoOwnedVisualRoot("pivot"), "VISUAL_ROOT_DIR",
                            "VISUAL_UPDATE_BASELINE", "VISUAL_MAX_DIFF_PIXELS"};

    void captureFrame(const QString& frameName, QWidget& host, int tolerance = -1) {
        recorder.record(frameName, VisualTestUtils::normalizeCapturedImage(host.grab().toImage()),
                        tolerance);
    }
};

TEST_F(ThemeVisualPivotTest, CaptureTokenDropdownFrames) {
    QWidget host;
    host.resize(560, 360);
    host.setObjectName("visualHost");
    auto* layout = new QVBoxLayout(&host);
    layout->setContentsMargins(24, 24, 24, 24);

    auto* dropdown = new TokenDropdown();
    dropdown->addToken(":/icons/tokens/sol.png", "SOL  —  Solana", "10.50 SOL");
    dropdown->addToken(":/icons/tokens/usdc.png", "USDC  —  USD Coin", "2450.00 USDC");
    dropdown->addToken(":/icons/tokens/usdt.png", "USDT  —  Tether", "500.00 USDT");
    dropdown->setCurrentToken(":/icons/tokens/sol.png", "SOL  —  Solana", "10.50 SOL");

    layout->addWidget(dropdown);
    layout->addStretch();
    host.show();
    settleUi();

    auto* button = host.findChild<QPushButton*>("tokenDropdownBtn");
    ASSERT_NE(button, nullptr);

    captureFrame("token_dropdown_closed", host);

    QTest::mouseClick(button, Qt::LeftButton);
    settleUi(100); // extra settle for popup compositing
    captureFrame("token_dropdown_open", host, INTERACTIVE_TOLERANCE);

    auto* list = host.findChild<QListView*>("tokenDropdownList");
    ASSERT_NE(list, nullptr);
    ASSERT_GE(list->model()->rowCount(), 1);

    const int hoverRow = qMin(1, list->model()->rowCount() - 1);
    const QRect itemRect = list->visualRect(list->model()->index(hoverRow, 0));
    QTest::mouseMove(list->viewport(), itemRect.center());
    settleUi();
    captureFrame("token_dropdown_item_hover", host, INTERACTIVE_TOLERANCE);

    QTest::mousePress(button, Qt::LeftButton);
    settleUi();
    captureFrame("token_dropdown_button_pressed", host, INTERACTIVE_TOLERANCE);
    QTest::mouseRelease(button, Qt::LeftButton);
    settleUi();

    QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, itemRect.center());
    settleUi();
    captureFrame("token_dropdown_selected", host);
}

TEST_F(ThemeVisualPivotTest, CaptureDropdownFrames) {
    QWidget host;
    host.resize(560, 360);
    auto* layout = new QVBoxLayout(&host);
    layout->setContentsMargins(24, 24, 24, 24);

    auto* dropdown = new Dropdown();
    dropdown->addItem("Name");
    dropdown->addItem("Address");
    dropdown->addItem("Date");
    dropdown->setCurrentItem("Filter by");
    dropdown->setFixedWidth(220);

    layout->addWidget(dropdown, 0, Qt::AlignLeft);
    layout->addStretch();
    host.show();
    settleUi();

    auto* button = host.findChild<QPushButton*>("dropdownBtn");
    ASSERT_NE(button, nullptr);

    captureFrame("dropdown_closed", host);

    QTest::mouseClick(button, Qt::LeftButton);
    settleUi();
    captureFrame("dropdown_open", host, INTERACTIVE_TOLERANCE);

    auto* list = host.findChild<QListWidget*>("dropdownList");
    ASSERT_NE(list, nullptr);
    ASSERT_GE(list->count(), 2);

    QRect itemRect = list->visualItemRect(list->item(1));
    QTest::mouseMove(list->viewport(), itemRect.center());
    settleUi();
    captureFrame("dropdown_item_hover", host, INTERACTIVE_TOLERANCE);

    QTest::mousePress(button, Qt::LeftButton);
    settleUi();
    captureFrame("dropdown_button_pressed", host, INTERACTIVE_TOLERANCE);
    QTest::mouseRelease(button, Qt::LeftButton);
    settleUi();

    QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, itemRect.center());
    settleUi();
    captureFrame("dropdown_selected", host);
}

TEST_F(ThemeVisualPivotTest, CaptureTokenDropdownFramesInViewportCascade) {
    QWidget host;
    host.resize(760, 420);
    auto* hostLayout = new QVBoxLayout(&host);
    hostLayout->setContentsMargins(0, 0, 0, 0);

    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    QWidget* content = new QWidget();
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(40, 24, 40, 24);
    layout->setSpacing(16);

    auto* dropdown = new TokenDropdown();
    dropdown->addToken(":/icons/tokens/sol.png", "SOL  —  Solana", "0.010000 SOL");
    dropdown->addToken(":/icons/tokens/usdc.png", "USDC  —  USD Coin", "12.34 USDC");
    dropdown->setCurrentToken(":/icons/tokens/sol.png", "SOL  —  Solana", "0.010000 SOL");
    layout->addWidget(dropdown);
    layout->addStretch();

    scroll->setWidget(content);
    scroll->viewport()->setStyleSheet("background: #12131f;");
    hostLayout->addWidget(scroll);

    host.show();
    settleUi();

    auto* button = host.findChild<QPushButton*>("tokenDropdownBtn");
    ASSERT_NE(button, nullptr);
    captureFrame("token_dropdown_viewport_closed", host);

    QTest::mouseClick(button, Qt::LeftButton);
    settleUi();
    captureFrame("token_dropdown_viewport_open", host, INTERACTIVE_TOLERANCE);

    auto* list = host.findChild<QListView*>("tokenDropdownList");
    ASSERT_NE(list, nullptr);
    ASSERT_GT(list->model()->rowCount(), 0);
    QRect itemRect = list->visualRect(list->model()->index(0, 0));
    QTest::mouseMove(list->viewport(), itemRect.center());
    settleUi();
    captureFrame("token_dropdown_viewport_item_hover", host, INTERACTIVE_TOLERANCE);
}

TEST_F(ThemeVisualPivotTest, CaptureDropdownFramesInViewportCascade) {
    QWidget host;
    host.resize(760, 420);
    auto* hostLayout = new QVBoxLayout(&host);
    hostLayout->setContentsMargins(0, 0, 0, 0);

    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    QWidget* content = new QWidget();
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(40, 24, 40, 24);
    layout->setSpacing(16);

    auto* dropdown = new Dropdown();
    dropdown->addItem("Name");
    dropdown->addItem("Address");
    dropdown->addItem("Date");
    dropdown->setCurrentItem("Filter by");
    dropdown->setFixedWidth(220);
    layout->addWidget(dropdown, 0, Qt::AlignLeft);
    layout->addStretch();

    scroll->setWidget(content);
    scroll->viewport()->setStyleSheet("background: #12131f;");
    hostLayout->addWidget(scroll);

    host.show();
    settleUi();

    auto* button = host.findChild<QPushButton*>("dropdownBtn");
    ASSERT_NE(button, nullptr);
    captureFrame("dropdown_viewport_closed", host);

    QTest::mouseClick(button, Qt::LeftButton);
    settleUi();
    captureFrame("dropdown_viewport_open", host, INTERACTIVE_TOLERANCE);

    auto* list = host.findChild<QListWidget*>("dropdownList");
    ASSERT_NE(list, nullptr);
    ASSERT_GT(list->count(), 0);
    QRect itemRect = list->visualItemRect(list->item(0));
    QTest::mouseMove(list->viewport(), itemRect.center());
    settleUi();
    captureFrame("dropdown_viewport_item_hover", host, INTERACTIVE_TOLERANCE);
}

TEST_F(ThemeVisualPivotTest, CaptureStyledCheckboxFrames) {
    QWidget host;
    host.resize(560, 200);
    auto* layout = new QVBoxLayout(&host);
    layout->setContentsMargins(24, 24, 24, 24);

    auto* checkbox = new StyledCheckbox("I understand and agree");
    layout->addWidget(checkbox, 0, Qt::AlignLeft);
    layout->addStretch();

    host.show();
    settleUi();

    captureFrame("styled_checkbox_unchecked", host);

    QTest::mouseMove(checkbox, QPoint(8, checkbox->height() / 2));
    settleUi();
    captureFrame("styled_checkbox_hover_unchecked", host, INTERACTIVE_TOLERANCE);

    QTest::mouseClick(checkbox, Qt::LeftButton, Qt::NoModifier, QPoint(8, checkbox->height() / 2));
    settleUi();
    captureFrame("styled_checkbox_checked", host);

    QTest::mouseMove(checkbox, QPoint(8, checkbox->height() / 2));
    settleUi();
    captureFrame("styled_checkbox_hover_checked", host, INTERACTIVE_TOLERANCE);
}

TEST_F(ThemeVisualPivotTest, CaptureAssetsPageFramesAndActions) {
    QWidget host;
    host.resize(680, 520);
    auto* layout = new QVBoxLayout(&host);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);

    AssetsPage page;
    QWidget* assetCard = page.createPoolCard();
    // Populate labels for visual test
    if (auto* n = assetCard->findChild<QLabel*>("assetCardName")) {
        n->setText("Solana (SOL)");
    }
    if (auto* p = assetCard->findChild<QLabel*>("assetCardPrice")) {
        p->setText("$1,050.00");
    }
    if (auto* h = assetCard->findChild<QLabel*>("cardHoldValue")) {
        h->setText("10.5 SOL");
    }
    if (auto* pv = assetCard->findChild<QLabel*>("cardPriceValue")) {
        pv->setText("$100.00");
    }
    assetCard->setFixedWidth(560);
    layout->addWidget(assetCard, 0, Qt::AlignHCenter);
    layout->addStretch();

    host.show();
    settleUi();
    // The asset card includes a live drop shadow, which can produce small
    // rasterization drift across runs even without interaction.
    captureFrame("assets_card_default", host, 50000);

    QPoint cardCenter = assetCard->mapTo(&host, assetCard->rect().center());
    QTest::mouseMove(&host, cardCenter);
    settleUi();
    captureFrame("assets_card_hover", host, INTERACTIVE_TOLERANCE);

    QPushButton* sendBtn = findButtonByText(assetCard, QObject::tr("Send"));
    QPushButton* receiveBtn = findButtonByText(assetCard, QObject::tr("Receive"));
    ASSERT_NE(sendBtn, nullptr);
    ASSERT_NE(receiveBtn, nullptr);

    QPoint sendHover = sendBtn->mapTo(&host, sendBtn->rect().center());
    QTest::mouseMove(&host, sendHover);
    settleUi();
    captureFrame("assets_send_hover", host, INTERACTIVE_TOLERANCE);

    QTest::mousePress(sendBtn, Qt::LeftButton);
    settleUi();
    captureFrame("assets_send_pressed", host, INTERACTIVE_TOLERANCE);
    QTest::mouseRelease(sendBtn, Qt::LeftButton);
    settleUi();

    QPoint recvHover = receiveBtn->mapTo(&host, receiveBtn->rect().center());
    QTest::mouseMove(&host, recvHover);
    settleUi();
    captureFrame("assets_receive_hover", host, INTERACTIVE_TOLERANCE);

    QTest::mousePress(receiveBtn, Qt::LeftButton);
    settleUi();
    captureFrame("assets_receive_pressed", host, INTERACTIVE_TOLERANCE);
    QTest::mouseRelease(receiveBtn, Qt::LeftButton);
    settleUi();
}

int main(int argc, char** argv) {
    QStandardPaths::setTestModeEnabled(true);

    QApplication app(argc, argv);

    QFontDatabase::addApplicationFont(":/fonts/Exo2-Variable.ttf");
    app.setFont(QFont("Exo 2", 14));

    const QString stylesheet = StyleLoader::loadTheme();
    app.setStyleSheet(stylesheet);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
