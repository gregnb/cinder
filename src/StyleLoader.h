#ifndef STYLELOADER_H
#define STYLELOADER_H

#include "Theme.h"
#include <QFile>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QTextStream>

namespace StyleLoader {

    inline QString replaceThemeTokens(QString stylesheet) {
        // Token → value map built from Theme constants
        const QMap<QString, QString> tokens = {
            // Backgrounds
            {"bgGradientStart", Theme::bgGradientStart},
            {"bgGradientMid", Theme::bgGradientMid},
            {"bgGradientEnd", Theme::bgGradientEnd},

            // Cards
            {"cardBgStart", Theme::cardBgStart},
            {"cardBgEnd", Theme::cardBgEnd},
            {"cardBgStartLight", Theme::cardBgStartLight},
            {"cardBgEndLight", Theme::cardBgEndLight},

            // Borders
            {"borderSubtle", Theme::borderSubtle},
            {"borderLight", Theme::borderLight},
            {"borderButton", Theme::borderButton},
            {"borderActive", Theme::borderActive},
            {"borderActiveHover", Theme::borderActiveHover},

            // Text
            {"textPrimary", Theme::textPrimary},
            {"textSecondary", Theme::textSecondary},
            {"textTertiary", Theme::textTertiary},
            {"textHover", Theme::textHover},

            // Accents
            {"successGreen", Theme::successGreen},
            {"errorRed", Theme::errorRed},
            {"solanaGreen", Theme::solanaGreen},
            {"solanaPurple", Theme::solanaPurple},
            {"solanaCyan", Theme::solanaCyan},

            // Hover
            {"hoverBg", Theme::hoverBg},
            {"hoverBgStrong", Theme::hoverBgStrong},
            {"overlayBg", Theme::overlayBg},

            // Dimensions
            {"cardRadius", QString::number(Theme::cardRadius)},
            {"buttonRadius", QString::number(Theme::buttonRadius)},
            {"smallRadius", QString::number(Theme::smallRadius)},

            // Font
            {"fontFamily", Theme::fontFamily},

            // Dropdown
            {"dropdownBg", Theme::dropdownBg},
            {"dropdownBorder", Theme::dropdownBorder},
            {"dropdownBorderHover", Theme::dropdownBorderHover},
            {"dropdownListBg", Theme::dropdownListBg},
            {"dropdownListBorder", Theme::dropdownListBorder},
            {"dropdownItemHover", Theme::dropdownItemHover},
            {"dropdownItemSelected", Theme::dropdownItemSelected},
            {"dropdownScrollHandle", Theme::dropdownScrollHandle},
            {"dropdownScrollHover", Theme::dropdownScrollHover},

            // Complex gradients
            {"chartGradient", Theme::chartGradient},
            {"stakingIconBg", Theme::stakingIconBg},
            {"stakingIconFallbackBg", Theme::stakingIconFallbackBg},
            {"txIconPositive", Theme::txIconPositive},
            {"txIconNegative", Theme::txIconNegative},
            {"activeNavGradient", Theme::activeNavGradient},
            {"activeNavGradientHover", Theme::activeNavGradientHover},
        };

        for (auto it = tokens.constBegin(); it != tokens.constEnd(); ++it) {
            stylesheet.replace("{{" + it.key() + "}}", it.value());
        }

        return stylesheet;
    }

    inline QString load(const QString& qssPath) {
        QFile file(qssPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return QString();

        QString stylesheet = QTextStream(&file).readAll();
        file.close();
        return replaceThemeTokens(stylesheet);
    }

    inline QString loadMany(const QStringList& qssPaths) {
        QString merged;
        for (const QString& path : qssPaths) {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;
            merged += QTextStream(&file).readAll();
            merged += "\n\n";
            file.close();
        }
        return replaceThemeTokens(merged);
    }

    inline QString loadTheme() {
        return loadMany({
            ":/styles/base.qss",
            ":/styles/transactions.qss",
            ":/styles/forms_wallet_addressbook.qss",
            ":/styles/assets.qss",
            ":/styles/dashboard.qss",
            ":/styles/activity.qss",
            ":/styles/staking.qss",
            ":/styles/swap.qss",
            ":/styles/settings.qss",
            ":/styles/sendreceive.qss",
            ":/styles/wallets.qss",
            ":/styles/lockscreen.qss",
            ":/styles/txlookup.qss",
            ":/styles/terminal.qss",
            ":/styles/notifications.qss",
            ":/styles/account_selector.qss",
            ":/styles/styled_calendar.qss",
            ":/styles/app_shell.qss",
        });
    }

} // namespace StyleLoader

#endif // STYLELOADER_H
