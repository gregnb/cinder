#include "crypto/plugin/trezor/TrezorProtobuf.h"
#include "crypto/plugin/trezor/TrezorTransport.h"

#include <QDebug>

// ── ProtobufReader ───────────────────────────────────────────

ProtobufReader::ProtobufReader(const QByteArray& data) : m_data(data) {}

uint64_t ProtobufReader::readVarint(const QByteArray& data, int& offset) {
    uint64_t result = 0;
    int shift = 0;
    while (offset < data.size()) {
        auto byte = static_cast<uint8_t>(data.at(offset++));
        result |= static_cast<uint64_t>(byte & 0x7F) << shift;
        if ((byte & 0x80) == 0) {
            break;
        }
        shift += 7;
        if (shift >= 64) {
            break;
        }
    }
    return result;
}

QByteArray ProtobufReader::readLengthDelimited(const QByteArray& data, int& offset) {
    auto length = static_cast<int>(readVarint(data, offset));
    if (offset + length > data.size()) {
        return {};
    }
    QByteArray result = data.mid(offset, length);
    offset += length;
    return result;
}

bool ProtobufReader::next(Field& field) {
    while (m_offset < m_data.size()) {
        auto tag = readVarint(m_data, m_offset);
        field.number = static_cast<int>(tag >> 3);
        field.wireType = static_cast<int>(tag & 0x07);
        field.varintVal = 0;
        field.bytesVal.clear();

        switch (field.wireType) {
            case 0: // varint
                field.varintVal = readVarint(m_data, m_offset);
                return true;
            case 2: // length-delimited (bytes, string, embedded message)
                field.bytesVal = readLengthDelimited(m_data, m_offset);
                return true;
            case 5: // fixed32
                m_offset += 4;
                continue; // skip
            case 1:       // fixed64
                m_offset += 8;
                continue; // skip
            default:
                return false; // unknown wire type — stop
        }
    }
    return false;
}

// ── Low-level protobuf encoding ──────────────────────────────

namespace TrezorProtobuf {

    QByteArray encodeVarint(uint64_t value) {
        QByteArray result;
        while (value > 0x7F) {
            result.append(static_cast<char>((value & 0x7F) | 0x80));
            value >>= 7;
        }
        result.append(static_cast<char>(value & 0x7F));
        return result;
    }

    QByteArray encodeTag(int fieldNumber, int wireType) {
        return encodeVarint(static_cast<uint64_t>((fieldNumber << 3) | wireType));
    }

    QByteArray encodeLengthDelimited(int fieldNumber, const QByteArray& data) {
        QByteArray result;
        result.append(encodeTag(fieldNumber, 2));
        result.append(encodeVarint(static_cast<uint64_t>(data.size())));
        result.append(data);
        return result;
    }

    QByteArray encodeUint32Field(int fieldNumber, uint32_t value) {
        QByteArray result;
        result.append(encodeTag(fieldNumber, 0));
        result.append(encodeVarint(value));
        return result;
    }

    QByteArray encodeBoolField(int fieldNumber, bool value) {
        QByteArray result;
        result.append(encodeTag(fieldNumber, 0));
        result.append(encodeVarint(value ? 1 : 0));
        return result;
    }

    QByteArray encodeStringField(int fieldNumber, const QString& value) {
        return encodeLengthDelimited(fieldNumber, value.toUtf8());
    }

    // ── BIP44 derivation path ────────────────────────────────────

    QList<uint32_t> parseBip44Path(const QString& path) {
        QString p = path;
        if (p.startsWith(QLatin1String("m/"))) {
            p = p.mid(2);
        }
        QStringList parts = p.split(QLatin1Char('/'));
        QList<uint32_t> result;
        for (const QString& part : parts) {
            QString seg = part.trimmed();
            bool hardened = seg.endsWith(QLatin1Char('\'')) || seg.endsWith(QLatin1Char('h'));
            if (hardened) {
                seg.chop(1);
            }
            bool ok = false;
            uint32_t val = seg.toUInt(&ok);
            if (!ok) {
                return {};
            }
            if (hardened) {
                val |= TrezorCrypto::kHardenedBit;
            }
            result.append(val);
        }
        return result;
    }

    QByteArray encodeAddressN(const QList<uint32_t>& path) {
        QByteArray result;
        for (uint32_t val : path) {
            result.append(encodeUint32Field(TrezorFields::Common::kAddressN, val));
        }
        return result;
    }

    // ── Message encoders ─────────────────────────────────────────

    QByteArray encodeInitialize() { return {}; }
    QByteArray encodeButtonAck() { return {}; }

    QByteArray encodePassphraseAck(const QString& passphrase) {
        if (passphrase.isEmpty()) {
            return encodeStringField(TrezorFields::PassphraseAck::kPassphrase, QString());
        }
        return encodeStringField(TrezorFields::PassphraseAck::kPassphrase, passphrase);
    }

    QByteArray encodePinMatrixAck(const QString& pin) {
        return encodeStringField(TrezorFields::PassphraseAck::kPassphrase, pin);
    }

    QByteArray encodeGetPublicKey(const QList<uint32_t>& addressN, const QString& curveName,
                                  bool showDisplay) {
        QByteArray result;
        result.append(encodeAddressN(addressN));
        if (!curveName.isEmpty()) {
            result.append(encodeStringField(TrezorFields::GetPublicKey::kCurveName, curveName));
        }
        if (showDisplay) {
            result.append(encodeBoolField(TrezorFields::GetPublicKey::kShowDisplay, true));
        }
        return result;
    }

    QByteArray encodeSolanaGetPublicKey(const QList<uint32_t>& addressN, bool showDisplay) {
        QByteArray result;
        result.append(encodeAddressN(addressN));
        if (showDisplay) {
            result.append(encodeBoolField(TrezorFields::Common::kShowDisplay, true));
        }
        return result;
    }

    QByteArray encodeSolanaGetAddress(const QList<uint32_t>& addressN, bool showDisplay) {
        QByteArray result;
        result.append(encodeAddressN(addressN));
        if (showDisplay) {
            result.append(encodeBoolField(TrezorFields::Common::kShowDisplay, true));
        }
        return result;
    }

    QByteArray encodeSolanaSignTx(const QList<uint32_t>& addressN, const QByteArray& serializedTx) {
        QByteArray result;
        result.append(encodeAddressN(addressN));
        result.append(
            encodeLengthDelimited(TrezorFields::SolanaSignTx::kSerializedTx, serializedTx));
        return result;
    }

    // ── Message decoders ─────────────────────────────────────────

    TrezorFeatures decodeFeatures(const QByteArray& data) {
        TrezorFeatures features;
        ProtobufReader reader(data);
        ProtobufReader::Field f;
        while (reader.next(f)) {
            switch (f.number) {
                case TrezorFields::Features::kMajorVersion:
                    features.majorVersion = f.toInt32();
                    break;
                case TrezorFields::Features::kMinorVersion:
                    features.minorVersion = f.toInt32();
                    break;
                case TrezorFields::Features::kPatchVersion:
                    features.patchVersion = f.toInt32();
                    break;
                case TrezorFields::Features::kPinProtection:
                    features.pinProtection = f.toBool();
                    break;
                case TrezorFields::Features::kInitialized:
                    features.initialized = f.toBool();
                    break;
                case TrezorFields::Features::kDeviceId:
                    features.deviceId = f.toString();
                    break;
                case TrezorFields::Features::kPassphraseProtection:
                    features.passphraseProtection = f.toBool();
                    break;
                case TrezorFields::Features::kModel:
                    features.model = f.toString();
                    break;
                case TrezorFields::Features::kLabel:
                    features.label = f.toString();
                    break;
                case TrezorFields::Features::kUnlocked:
                    features.unlocked = f.toBool();
                    break;
                case TrezorFields::Features::kInternalModel:
                    features.internalModel = f.toString();
                    break;
                default:
                    break;
            }
        }
        return features;
    }

    TrezorFailure decodeFailure(const QByteArray& data) {
        TrezorFailure failure;
        ProtobufReader reader(data);
        ProtobufReader::Field f;
        while (reader.next(f)) {
            switch (f.number) {
                case TrezorFields::Failure::kCode:
                    failure.code = f.toInt32();
                    break;
                case TrezorFields::Failure::kMessage:
                    failure.message = f.toString();
                    break;
                default:
                    break;
            }
        }
        return failure;
    }

    QByteArray decodePublicKeyNode(const QByteArray& data) {
        ProtobufReader reader(data);
        ProtobufReader::Field f;
        while (reader.next(f)) {
            if (f.number == TrezorFields::PublicKey::kNode) {
                // Parse embedded HDNodeType
                ProtobufReader nodeReader = f.toMessage();
                ProtobufReader::Field nf;
                while (nodeReader.next(nf)) {
                    if (nf.number == TrezorFields::HDNodeType::kPublicKey) {
                        QByteArray pubkey = nf.toBytes();
                        // ed25519: 33 bytes with 0x00 prefix → strip to 32
                        if (pubkey.size() == TrezorCrypto::kEd25519PaddedKeySize &&
                            static_cast<uint8_t>(pubkey.at(0)) == TrezorCrypto::kEd25519PadByte) {
                            return pubkey.mid(1, TrezorCrypto::kEd25519KeySize);
                        }
                        return pubkey;
                    }
                }
            }
        }
        return {};
    }

    QByteArray decodeSolanaPublicKey(const QByteArray& data) {
        ProtobufReader reader(data);
        ProtobufReader::Field f;
        while (reader.next(f)) {
            if (f.number == TrezorFields::SolanaPublicKey::kPublicKey) {
                return f.toBytes();
            }
        }
        return {};
    }

    QString decodeSolanaAddress(const QByteArray& data) {
        ProtobufReader reader(data);
        ProtobufReader::Field f;
        while (reader.next(f)) {
            if (f.number == TrezorFields::SolanaAddress::kAddress) {
                return f.toString();
            }
        }
        return {};
    }

    QByteArray decodeSolanaTxSignature(const QByteArray& data) {
        ProtobufReader reader(data);
        ProtobufReader::Field f;
        while (reader.next(f)) {
            if (f.number == TrezorFields::SolanaTxSignature::kSignature) {
                return f.toBytes();
            }
        }
        return {};
    }

    // ── Interaction helper ───────────────────────────────────────

    TrezorResponse driveInteraction(TrezorTransport* transport, TrezorResponse initial,
                                    int timeoutMs) {
        TrezorResponse resp = std::move(initial);
        for (int i = 0; i < kMaxInteractionLoops; ++i) {
            if (resp.msgType == TrezorMsg::ButtonRequest) {
                qDebug() << "[driveInteraction] ButtonRequest #" << (i + 1);
                resp = transport->call(TrezorMsg::ButtonAck, encodeButtonAck(), timeoutMs);
            } else if (resp.msgType == TrezorMsg::PinMatrixRequest) {
                // Model T/Safe 3 show PIN on touchscreen — host just acknowledges.
                // Model One would need a real PIN matrix UI (not yet supported).
                qDebug() << "[driveInteraction] PinMatrixRequest #" << (i + 1)
                         << "— sending empty ack (touchscreen PIN)";
                resp = transport->call(TrezorMsg::PinMatrixAck, encodePinMatrixAck(QString()),
                                       timeoutMs);
            } else if (resp.msgType == TrezorMsg::PassphraseRequest) {
                qDebug() << "[driveInteraction] PassphraseRequest #" << (i + 1);
                resp = transport->call(TrezorMsg::PassphraseAck, encodePassphraseAck(), timeoutMs);
            } else {
                return resp;
            }
        }
        qWarning() << "[driveInteraction] Exceeded max interaction loops";
        return resp;
    }

} // namespace TrezorProtobuf
