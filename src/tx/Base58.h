#ifndef BASE58_H
#define BASE58_H

#include <QByteArray>
#include <QString>
#include <algorithm>

namespace Base58 {

    inline constexpr char ALPHABET[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

    inline QString encode(const QByteArray& bytes) {
        int leadingZeros = 0;
        for (int i = 0; i < bytes.size() && bytes[i] == 0; ++i) {
            ++leadingZeros;
        }

        QByteArray tmp = bytes;
        QString result;
        while (!tmp.isEmpty()) {
            int remainder = 0;
            QByteArray quotient;
            for (int i = 0; i < tmp.size(); ++i) {
                int value = remainder * 256 + static_cast<uchar>(tmp[i]);
                int digit = value / 58;
                remainder = value % 58;
                if (!quotient.isEmpty() || digit > 0) {
                    quotient.append(static_cast<char>(digit));
                }
            }
            result.prepend(ALPHABET[remainder]);
            tmp = quotient;
        }

        for (int i = 0; i < leadingZeros; ++i) {
            result.prepend('1');
        }

        return result;
    }

    inline QByteArray decode(const QString& str) {
        static int TABLE[128];
        static bool inited = false;
        if (!inited) {
            std::fill(std::begin(TABLE), std::end(TABLE), -1);
            for (int i = 0; i < 58; ++i) {
                TABLE[static_cast<int>(ALPHABET[i])] = i;
            }
            inited = true;
        }

        int leadingOnes = 0;
        for (int i = 0; i < str.size() && str[i] == QLatin1Char('1'); ++i) {
            ++leadingOnes;
        }

        QByteArray result;
        for (int i = 0; i < str.size(); ++i) {
            int c = str[i].toLatin1();
            if (c < 0 || c >= 128 || TABLE[c] < 0) {
                return {};
            }
            int carry = TABLE[c];
            for (int j = result.size() - 1; j >= 0; --j) {
                int val = static_cast<uchar>(result[j]) * 58 + carry;
                result[j] = static_cast<char>(val & 0xFF);
                carry = val >> 8;
            }
            while (carry > 0) {
                result.prepend(static_cast<char>(carry & 0xFF));
                carry >>= 8;
            }
        }

        QByteArray prefix(leadingOnes, '\0');
        return prefix + result;
    }

} // namespace Base58

#endif // BASE58_H
