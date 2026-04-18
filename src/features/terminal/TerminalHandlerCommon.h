#ifndef TERMINALHANDLERCOMMON_H
#define TERMINALHANDLERCOMMON_H

#include <QColor>
#include <QString>
#include <sodium.h>

namespace terminal {
    inline constexpr int kRpcTimeoutMs = 10000;

    inline const QColor kErrorColor(0xef, 0x44, 0x44);
    inline const QColor kDimColor(120, 130, 160);
    inline const QColor kPromptColor(0x14, 0xF1, 0x95);
    inline const QColor kWarnColor(250, 204, 21);
    inline const QColor kAccentColor(153, 69, 255);

    inline QString truncAddr(const QString& a) {
        return (a.length() > 12) ? a.left(4) + "..." + a.right(4) : a;
    }

    inline QString formatSol(quint64 lamports) {
        double sol = lamports / 1e9;
        return QString::number(sol, 'f', sol >= 1.0 ? 4 : 6) + " SOL";
    }

    inline QString formatLamports(qint64 lamports) {
        return QString::number(lamports) + " lamports";
    }

    inline QString padRight(const QString& s, int width) { return s.leftJustified(width, ' '); }

    inline void wipeQString(QString& s) {
        if (!s.isEmpty()) {
            sodium_memzero(s.data(), static_cast<size_t>(s.size() * sizeof(QChar)));
            s.clear();
        }
    }

    inline QString fmtKeyName(const QString& key) {
        if (key.isEmpty()) {
            return key;
        }
        QString r;
        for (int i = 0; i < key.length(); ++i) {
            if (i == 0) {
                r += key[i].toUpper();
            } else if (key[i] == '_') {
                r += ' ';
            } else if (key[i].isUpper()) {
                r += ' ';
                r += key[i];
            } else {
                r += key[i];
            }
        }
        return r;
    }

    inline QString fmtTypeName(const QString& type) {
        if (type.isEmpty()) {
            return "Unknown";
        }
        QString r;
        bool nextUp = true;
        for (int i = 0; i < type.length(); ++i) {
            if (type[i] == '_') {
                r += ' ';
                nextUp = true;
            } else if (nextUp) {
                r += type[i].toUpper();
                nextUp = false;
            } else if (type[i].isUpper()) {
                r += ' ';
                r += type[i];
            } else {
                r += type[i];
            }
        }
        return r;
    }

    inline bool looksLikeAddr(const QString& t) {
        if (t.length() < 32 || t.length() > 44) {
            return false;
        }
        static const QString kBase58 = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
        for (const QChar& ch : t) {
            if (!kBase58.contains(ch)) {
                return false;
            }
        }
        return true;
    }
} // namespace terminal

#endif // TERMINALHANDLERCOMMON_H
