#ifndef COMPACTU16_H
#define COMPACTU16_H

#include <QByteArray>
#include <cstdint>

namespace CompactU16 {

    inline void encode(uint16_t value, QByteArray& out) {
        if (value < 0x80) {
            out.append(static_cast<char>(value));
        } else if (value < 0x4000) {
            out.append(static_cast<char>((value & 0x7F) | 0x80));
            out.append(static_cast<char>(value >> 7));
        } else {
            out.append(static_cast<char>((value & 0x7F) | 0x80));
            out.append(static_cast<char>(((value >> 7) & 0x7F) | 0x80));
            out.append(static_cast<char>(value >> 14));
        }
    }

    inline int decode(const QByteArray& data, int& offset) {
        int value = 0;
        int shift = 0;
        for (int i = 0; i < 3; ++i) {
            if (offset >= data.size())
                return -1;
            uint8_t byte = static_cast<uint8_t>(data[offset++]);
            value |= (byte & 0x7F) << shift;
            if ((byte & 0x80) == 0)
                return value;
            shift += 7;
        }
        return -1;
    }

} // namespace CompactU16

#endif // COMPACTU16_H
