#ifndef THEME_H
#define THEME_H

#include <QColor>
#include <QString>

namespace Theme {

    // ── Background Gradient ──────────────────────────────────────
    inline const QString bgGradientStart = "#1a1b2e";
    inline const QString bgGradientMid = "#151628";
    inline const QString bgGradientEnd = "#12131f";

    // ── Card Backgrounds ─────────────────────────────────────────
    inline const QString cardBgStart = "rgba(30, 31, 55, 0.8)";
    inline const QString cardBgEnd = "rgba(25, 26, 45, 0.8)";
    inline const QString cardBgStartLight = "rgba(30, 31, 55, 0.6)";
    inline const QString cardBgEndLight = "rgba(25, 26, 45, 0.6)";

    // ── Borders ──────────────────────────────────────────────────
    inline const QString borderSubtle = "rgba(100, 100, 150, 0.2)";
    inline const QString borderLight = "rgba(100, 100, 150, 0.18)";
    inline const QString borderButton = "rgba(150, 150, 200, 0.3)";
    inline const QString borderActive = "rgba(140, 100, 230, 0.35)";
    inline const QString borderActiveHover = "rgba(140, 100, 230, 0.5)";

    // ── Text Colors ──────────────────────────────────────────────
    inline const QString textPrimary = "white";
    inline const QString textSecondary = "rgba(255, 255, 255, 0.6)";
    inline const QString textTertiary = "rgba(255, 255, 255, 0.5)";
    inline const QString textHover = "rgba(255, 255, 255, 0.85)";

    // ── Accent Colors ────────────────────────────────────────────
    inline const QString successGreen = "#10b981";
    inline const QString errorRed = "#ef4444";
    inline const QString solanaGreen = "#14F195";
    inline const QString solanaPurple = "#9945FF";
    inline const QString solanaCyan = "#00D4FF";

    // ── Hover / Overlay ──────────────────────────────────────────
    inline const QString hoverBg = "rgba(100, 100, 150, 0.15)";
    inline const QString hoverBgStrong = "rgba(100, 100, 150, 0.3)";
    inline const QString overlayBg = "rgba(100, 100, 150, 0.2)";

    // ── Dimensions ───────────────────────────────────────────────
    inline constexpr int cardRadius = 20;
    inline constexpr int buttonRadius = 16;
    inline constexpr int smallRadius = 8;
    inline constexpr int contentBorderRadius = 16;

    // ── Font ─────────────────────────────────────────────────────
    inline const QString fontFamily = R"("Exo 2", "Inter", -apple-system, sans-serif)";

    // ── QColor versions (for QPainter / QGraphicsDropShadowEffect) ──
    inline const QColor borderQColor = QColor(100, 100, 150, 46);
    inline const QColor shadowColor = QColor(0, 0, 0, 60);
    inline const QColor shadowColorLight = QColor(0, 0, 0, 50);
    inline const QColor glowColor = QColor(120, 80, 240, 190);
    inline const QColor logoGlowColor = QColor(153, 69, 255, 100);

    // ── Dropdown ────────────────────────────────────────────────────
    inline const QString dropdownBg = "#1a1e38";
    inline const QString dropdownBorder = "rgba(100, 100, 150, 0.3)";
    inline const QString dropdownBorderHover = "rgba(100, 150, 255, 0.5)";
    inline const QString dropdownListBg = "#161a30";
    inline const QString dropdownListBorder = "#2a3a5a";
    inline const QString dropdownItemHover = "#253050";
    inline const QString dropdownItemSelected = "#2a3a5a";
    inline const QString dropdownScrollHandle = "#3a4a6a";
    inline const QString dropdownScrollHover = "#4a5a7a";

    // ── Transaction Card Accent Colors ──────────────────────────────
    inline const QColor txAccentSendSol = QColor(59, 130, 246);
    inline const QColor txAccentSendToken = QColor(147, 51, 234);
    inline const QColor txAccentCreateToken = QColor(16, 185, 129);
    inline const QColor txAccentMintTokens = QColor(6, 182, 212);
    inline const QColor txAccentBurnTokens = QColor(245, 158, 11);
    inline const QColor txAccentCloseAccounts = QColor(239, 68, 68);

    // ── Active Nav Button Gradient (used in C++ logic) ───────────
    inline const QString activeNavGradient = R"(
    qlineargradient(x1:0, y1:0, x2:1, y2:0,
        stop:0 rgba(109, 45, 149, 0.5),
        stop:0.3 rgba(95, 67, 173, 0.5),
        stop:0.5 rgba(77, 97, 193, 0.5),
        stop:0.7 rgba(51, 123, 211, 0.5),
        stop:1 rgba(55, 129, 223, 0.5)))";

    inline const QString activeNavGradientHover = R"(
    qlineargradient(x1:0, y1:0, x2:1, y2:0,
        stop:0 rgba(109, 45, 149, 0.6),
        stop:0.3 rgba(95, 67, 173, 0.6),
        stop:0.5 rgba(77, 97, 193, 0.6),
        stop:0.7 rgba(51, 123, 211, 0.6),
        stop:1 rgba(55, 129, 223, 0.6)))";

    // ── Transaction Icon Backgrounds (used in C++ logic) ─────────
    inline const QString txIconPositive = R"(
    qlineargradient(x1:0, y1:0, x2:1, y2:1,
        stop:0 rgba(59, 130, 246, 0.6), stop:1 rgba(147, 51, 234, 0.6)))";

    inline const QString txIconNegative = R"(
    qlineargradient(x1:0, y1:0, x2:1, y2:1,
        stop:0 rgba(139, 92, 246, 0.5), stop:1 rgba(168, 85, 247, 0.5)))";

    inline const QString txIconNeutral = R"(
    qlineargradient(x1:0, y1:0, x2:1, y2:1,
        stop:0 rgba(100, 116, 139, 0.5), stop:1 rgba(71, 85, 105, 0.5)))";

    inline const QString txIconBurn = R"(
    qlineargradient(x1:0, y1:0, x2:1, y2:1,
        stop:0 rgba(239, 68, 68, 0.5), stop:1 rgba(220, 38, 38, 0.5)))";

    inline const QString txIconMint = R"(
    qlineargradient(x1:0, y1:0, x2:1, y2:1,
        stop:0 rgba(16, 185, 129, 0.5), stop:1 rgba(5, 150, 105, 0.5)))";

    // ── Chart Gradient ───────────────────────────────────────────
    inline const QString chartGradient = R"(
    qlineargradient(x1:0, y1:1, x2:1, y2:0,
        stop:0 rgba(6, 182, 212, 0.1),
        stop:0.3 rgba(59, 130, 246, 0.15),
        stop:0.6 rgba(139, 92, 246, 0.15),
        stop:1 rgba(168, 85, 247, 0.1)))";

    // ── Staking Icon Background ──────────────────────────────────
    inline const QString stakingIconBg = R"(
    qlineargradient(x1:0, y1:0, x2:1, y2:1,
        stop:0 rgba(20, 241, 149, 0.15), stop:1 rgba(153, 69, 255, 0.15)))";

    inline const QString stakingIconFallbackBg = R"(
    qlineargradient(x1:0, y1:0, x2:1, y2:1,
        stop:0 rgba(20, 241, 149, 1.0), stop:0.5 rgba(153, 69, 255, 1.0), stop:1 rgba(0, 212, 255, 1.0)))";

    // ── Primary Action Button (inline — needed inside QScrollArea viewports) ──
    // Viewport's unscoped setStyleSheet cascades to children, killing QSS backgrounds.
    // These constants define the inline style applied directly on buttons to override it.
    inline const QString primaryBtnStyle =
        "QPushButton {"
        "  background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "    stop:0 #1a3a5c, stop:0.5 #224a70, stop:1 #1a3a5c);"
        "  color: white; border: 1px solid #2a5a8a; border-radius: 12px;"
        "  padding: 14px; font-size: 16px; font-weight: 600;"
        "}"
        "QPushButton:hover {"
        "  background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "    stop:0 #224a70, stop:0.5 #2a5a8a, stop:1 #224a70);"
        "  border: 1px solid #3a6a9a;"
        "}";

    inline const QString primaryBtnDisabledStyle =
        "QPushButton {"
        "  background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "    stop:0 #2a1a4a, stop:0.5 #252050, stop:1 #1a2545);"
        "  color: rgba(255, 255, 255, 0.35); border: 1px solid transparent; border-radius: 12px;"
        "  padding: 14px; font-size: 16px; font-weight: 600;"
        "}";

    inline const QString destructiveBtnStyle =
        "QPushButton {"
        "  background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "    stop:0 #5c1a1a, stop:0.5 #702222, stop:1 #5c1a1a);"
        "  color: white; border: 1px solid #8a2a2a; border-radius: 12px;"
        "  padding: 14px; font-size: 16px; font-weight: 600;"
        "}"
        "QPushButton:hover {"
        "  background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "    stop:0 #702222, stop:0.5 #8a2a2a, stop:1 #702222);"
        "  border: 1px solid #9a3a3a;"
        "}";

    inline const QString destructiveBtnDisabledStyle =
        "QPushButton {"
        "  background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "    stop:0 #3a1a1a, stop:0.5 #352020, stop:1 #301a1a);"
        "  color: rgba(255, 255, 255, 0.35); border: 1px solid transparent; border-radius: 12px;"
        "  padding: 14px; font-size: 16px; font-weight: 600;"
        "}";

} // namespace Theme

#endif // THEME_H
