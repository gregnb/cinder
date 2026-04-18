#ifndef TREZORPROTOBUF_H
#define TREZORPROTOBUF_H

#include <QByteArray>
#include <QList>
#include <QString>
#include <cstdint>

// ── Trezor protobuf message type IDs ─────────────────────────
namespace TrezorMsg {
    constexpr uint16_t Initialize = 0;
    constexpr uint16_t Success = 2;
    constexpr uint16_t Failure = 3;
    constexpr uint16_t GetPublicKey = 11;
    constexpr uint16_t PublicKey = 12;
    constexpr uint16_t Features = 17;
    constexpr uint16_t PinMatrixRequest = 18;
    constexpr uint16_t PinMatrixAck = 19;
    constexpr uint16_t ButtonRequest = 26;
    constexpr uint16_t ButtonAck = 27;
    constexpr uint16_t PassphraseRequest = 41;
    constexpr uint16_t PassphraseAck = 42;
    // Solana-specific (correct IDs: 900-905, NOT 700-705)
    constexpr uint16_t SolanaGetPublicKey = 900;
    constexpr uint16_t SolanaPublicKey = 901;
    constexpr uint16_t SolanaGetAddress = 902;
    constexpr uint16_t SolanaAddress = 903;
    constexpr uint16_t SolanaSignTx = 904;
    constexpr uint16_t SolanaTxSignature = 905;
} // namespace TrezorMsg

// ── Named protobuf field numbers ─────────────────────────────
// Organized by message type, matching trezor-common .proto definitions.
namespace TrezorFields {
    namespace Common {
        constexpr int kAddressN = 1;
        constexpr int kShowDisplay = 2;
    } // namespace Common
    // Field numbers from trezor-firmware/common/protob/messages-management.proto
    namespace Features {
        constexpr int kMajorVersion = 2;
        constexpr int kMinorVersion = 3;
        constexpr int kPatchVersion = 4;
        constexpr int kDeviceId = 6;
        constexpr int kPinProtection = 7;
        constexpr int kPassphraseProtection = 8;
        constexpr int kLabel = 10;
        constexpr int kInitialized = 12;
        constexpr int kUnlocked = 16;
        constexpr int kModel = 21;
        constexpr int kInternalModel = 44;
    } // namespace Features
    namespace Failure {
        constexpr int kCode = 1;
        constexpr int kMessage = 2;
    } // namespace Failure
    namespace GetPublicKey {
        constexpr int kCurveName = 2;
        constexpr int kShowDisplay = 5;
    } // namespace GetPublicKey
    namespace PublicKey {
        constexpr int kNode = 1;
    }
    namespace HDNodeType {
        constexpr int kPublicKey = 4;
    }
    namespace SolanaPublicKey {
        constexpr int kPublicKey = 1;
    }
    namespace SolanaAddress {
        constexpr int kAddress = 1;
    }
    namespace SolanaSignTx {
        constexpr int kSerializedTx = 2;
    }
    namespace SolanaTxSignature {
        constexpr int kSignature = 1;
    }
    namespace PassphraseAck {
        constexpr int kPassphrase = 1;
    }
} // namespace TrezorFields

// ── Wire protocol constants ──────────────────────────────────
namespace TrezorWire {
    constexpr uint8_t kMarker = 0x3F; // '?' packet marker
    constexpr char kHeaderMagic0 = '#';
    constexpr char kHeaderMagic1 = '#';
    constexpr int kHeaderSize = 8; // "##" + type(2) + len(4)
    constexpr int kUsbPacketSize = 64;
    constexpr int kHidWriteSize = 65; // +1 for report ID
    constexpr uint8_t kHidReportId = 0x00;
    constexpr int kPayloadPerPacket = 63; // packetSize - marker byte
    constexpr int kFirstPayload = 55;     // packetSize - marker - header
} // namespace TrezorWire

// ── Crypto constants ─────────────────────────────────────────
namespace TrezorCrypto {
    constexpr int kEd25519KeySize = 32;
    constexpr int kEd25519PaddedKeySize = 33; // SLIP-0010: 0x00 + 32 bytes
    constexpr uint8_t kEd25519PadByte = 0x00;
    constexpr int kEd25519SignatureSize = 64;
    constexpr uint32_t kHardenedBit = 0x80000000;
} // namespace TrezorCrypto

// ── Timeout constants ────────────────────────────────────────
namespace TrezorTimeout {
    constexpr int kInit = 10000;
    constexpr int kDefault = 30000;
    constexpr int kButtonConfirm = 60000;
    constexpr int kSign = 120000;
    constexpr int kUsbWrite = 5000;
} // namespace TrezorTimeout

constexpr int kMaxInteractionLoops = 20;

// ── Decoded message structs ──────────────────────────────────

struct TrezorFeatures {
    QString model;
    QString internalModel; // e.g. "T2B1" (Safe 3), "T3T1" (Safe 5)
    QString label;
    QString deviceId;
    int majorVersion = 0;
    int minorVersion = 0;
    int patchVersion = 0;
    bool initialized = false;
    bool pinProtection = false;
    bool passphraseProtection = false;
    bool unlocked = false;

    QString firmwareVersionString() const {
        return QStringLiteral("%1.%2.%3").arg(majorVersion).arg(minorVersion).arg(patchVersion);
    }
};

struct TrezorFailure {
    int code = 0;
    QString message;
};

// ── ProtobufReader ───────────────────────────────────────────
// Structured field iterator that eliminates duplicated skip logic.
// Reads tag + value in next(), skips unhandled fields automatically.

class ProtobufReader {
  public:
    explicit ProtobufReader(const QByteArray& data);

    struct Field {
        int number = 0;
        int wireType = 0;
        uint64_t varintVal = 0;
        QByteArray bytesVal;

        uint64_t toUInt64() const { return varintVal; }
        int32_t toInt32() const { return static_cast<int32_t>(varintVal); }
        bool toBool() const { return varintVal != 0; }
        QByteArray toBytes() const { return bytesVal; }
        QString toString() const { return QString::fromUtf8(bytesVal); }
        ProtobufReader toMessage() const { return ProtobufReader(bytesVal); }
    };

    bool next(Field& field);

  private:
    static uint64_t readVarint(const QByteArray& data, int& offset);
    static QByteArray readLengthDelimited(const QByteArray& data, int& offset);

    QByteArray m_data;
    int m_offset = 0;
};

// ── TrezorProtobuf namespace ─────────────────────────────────

struct TrezorResponse;
class TrezorTransport;

namespace TrezorProtobuf {

    // ── Low-level protobuf encoding ──────────────────────
    QByteArray encodeVarint(uint64_t value);
    QByteArray encodeTag(int fieldNumber, int wireType);
    QByteArray encodeLengthDelimited(int fieldNumber, const QByteArray& data);
    QByteArray encodeUint32Field(int fieldNumber, uint32_t value);
    QByteArray encodeBoolField(int fieldNumber, bool value);
    QByteArray encodeStringField(int fieldNumber, const QString& value);

    // ── BIP44 derivation path ────────────────────────────
    QList<uint32_t> parseBip44Path(const QString& path);
    QByteArray encodeAddressN(const QList<uint32_t>& path);

    // ── Message encoders ─────────────────────────────────
    QByteArray encodeInitialize();
    QByteArray encodeButtonAck();
    QByteArray encodePassphraseAck(const QString& passphrase = {});
    QByteArray encodePinMatrixAck(const QString& pin);
    QByteArray encodeGetPublicKey(const QList<uint32_t>& addressN,
                                  const QString& curveName = QStringLiteral("ed25519"),
                                  bool showDisplay = false);
    QByteArray encodeSolanaGetPublicKey(const QList<uint32_t>& addressN, bool showDisplay = false);
    QByteArray encodeSolanaGetAddress(const QList<uint32_t>& addressN, bool showDisplay = false);
    QByteArray encodeSolanaSignTx(const QList<uint32_t>& addressN, const QByteArray& serializedTx);

    // ── Message decoders ─────────────────────────────────
    TrezorFeatures decodeFeatures(const QByteArray& data);
    TrezorFailure decodeFailure(const QByteArray& data);
    QByteArray decodePublicKeyNode(const QByteArray& data);
    QByteArray decodeSolanaPublicKey(const QByteArray& data);
    QString decodeSolanaAddress(const QByteArray& data);
    QByteArray decodeSolanaTxSignature(const QByteArray& data);

    // ── Interaction helper ───────────────────────────────
    // Drives the ButtonRequest/PassphraseRequest loop until a final response.
    TrezorResponse driveInteraction(TrezorTransport* transport, TrezorResponse initial,
                                    int timeoutMs = TrezorTimeout::kButtonConfirm);

} // namespace TrezorProtobuf

#endif // TREZORPROTOBUF_H
