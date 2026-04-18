#include "crypto/plugin/trezor/TrezorProtobuf.h"

#include <gtest/gtest.h>

static QByteArray fromHex(const char* hex) { return QByteArray::fromHex(QByteArray(hex)); }

// Known Trezor Safe 3 test values (derivation path m/44'/501'/0'/0')
static const char* kExpectedPubkeyHex =
    "f276122d477c6df38156d8763c6a4f95666984300bdbc47d7833a9089b58bfad";
static const char* kExpectedAddress = "HKU5xTkGeZ5BM9vtVScFUBv68oxA4wa4pbPkWxRwyAgt";

// ══════════════════════════════════════════════════════════════
// Varint encoding / decoding round-trips
// ══════════════════════════════════════════════════════════════

class VarintTest : public ::testing::TestWithParam<std::tuple<uint64_t, const char*>> {};

TEST_P(VarintTest, EncodeMatchesExpectedHex) {
    auto [value, expectedHex] = GetParam();
    QByteArray encoded = TrezorProtobuf::encodeVarint(value);
    EXPECT_EQ(encoded.toHex(), QByteArray(expectedHex));
}

TEST_P(VarintTest, DecodeRoundTrips) {
    auto [value, _] = GetParam();
    QByteArray encoded = TrezorProtobuf::encodeVarint(value);
    int offset = 0;
    // Use ProtobufReader's varint logic by doing a full field encode/decode
    QByteArray fieldData = TrezorProtobuf::encodeUint32Field(1, static_cast<uint32_t>(value));
    ProtobufReader reader(fieldData);
    ProtobufReader::Field f;
    ASSERT_TRUE(reader.next(f));
    EXPECT_EQ(f.number, 1);
    EXPECT_EQ(f.toUInt64(), value & 0xFFFFFFFF); // uint32 truncation
}

INSTANTIATE_TEST_SUITE_P(VarintEdgeCases, VarintTest,
                         ::testing::Values(std::make_tuple(uint64_t(0), "00"),
                                           std::make_tuple(uint64_t(1), "01"),
                                           std::make_tuple(uint64_t(127), "7f"),
                                           std::make_tuple(uint64_t(128), "8001"),
                                           std::make_tuple(uint64_t(300), "ac02"),
                                           std::make_tuple(uint64_t(16383), "ff7f"),
                                           std::make_tuple(uint64_t(16384), "808001")));

// ══════════════════════════════════════════════════════════════
// BIP44 path parsing
// ══════════════════════════════════════════════════════════════

TEST(Bip44PathTest, TrezorFourLevelPath) {
    auto result = TrezorProtobuf::parseBip44Path(QStringLiteral("m/44'/501'/0'/0'"));
    ASSERT_EQ(result.size(), 4);
    EXPECT_EQ(result[0], 44 | TrezorCrypto::kHardenedBit);
    EXPECT_EQ(result[1], 501 | TrezorCrypto::kHardenedBit);
    EXPECT_EQ(result[2], 0 | TrezorCrypto::kHardenedBit);
    EXPECT_EQ(result[3], 0 | TrezorCrypto::kHardenedBit);
}

TEST(Bip44PathTest, LedgerThreeLevelPath) {
    auto result = TrezorProtobuf::parseBip44Path(QStringLiteral("m/44'/501'/0'"));
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], 44 | TrezorCrypto::kHardenedBit);
    EXPECT_EQ(result[1], 501 | TrezorCrypto::kHardenedBit);
    EXPECT_EQ(result[2], 0 | TrezorCrypto::kHardenedBit);
}

TEST(Bip44PathTest, HNotation) {
    auto result = TrezorProtobuf::parseBip44Path(QStringLiteral("m/44h/501h/0h"));
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], 44 | TrezorCrypto::kHardenedBit);
    EXPECT_EQ(result[1], 501 | TrezorCrypto::kHardenedBit);
    EXPECT_EQ(result[2], 0 | TrezorCrypto::kHardenedBit);
}

TEST(Bip44PathTest, NonHardenedComponent) {
    auto result = TrezorProtobuf::parseBip44Path(QStringLiteral("m/44'/501'/0'/0"));
    ASSERT_EQ(result.size(), 4);
    EXPECT_EQ(result[3], uint32_t(0)); // NOT hardened
}

TEST(Bip44PathTest, WithoutMPrefix) {
    auto result = TrezorProtobuf::parseBip44Path(QStringLiteral("44'/501'/0'"));
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], 44 | TrezorCrypto::kHardenedBit);
}

TEST(Bip44PathTest, AccountIndex1) {
    auto result = TrezorProtobuf::parseBip44Path(QStringLiteral("m/44'/501'/1'"));
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[2], 1 | TrezorCrypto::kHardenedBit);
}

TEST(Bip44PathTest, EmptyStringReturnsEmpty) {
    EXPECT_TRUE(TrezorProtobuf::parseBip44Path(QString()).isEmpty());
}

TEST(Bip44PathTest, InvalidSegmentReturnsEmpty) {
    EXPECT_TRUE(TrezorProtobuf::parseBip44Path(QStringLiteral("m/44'/abc/0'")).isEmpty());
}

TEST(Bip44PathTest, JustMSlashReturnsEmpty) {
    EXPECT_TRUE(TrezorProtobuf::parseBip44Path(QStringLiteral("m/")).isEmpty());
}

// ══════════════════════════════════════════════════════════════
// ProtobufReader
// ══════════════════════════════════════════════════════════════

TEST(ProtobufReaderTest, EmptyDataYieldsNoFields) {
    QByteArray empty;
    ProtobufReader reader(empty);
    ProtobufReader::Field f;
    EXPECT_FALSE(reader.next(f));
}

TEST(ProtobufReaderTest, SingleVarintField) {
    // field 1, wire type 0, value 42
    QByteArray data = TrezorProtobuf::encodeUint32Field(1, 42);
    ProtobufReader reader(data);
    ProtobufReader::Field f;
    ASSERT_TRUE(reader.next(f));
    EXPECT_EQ(f.number, 1);
    EXPECT_EQ(f.wireType, 0);
    EXPECT_EQ(f.toUInt64(), 42u);
    EXPECT_FALSE(reader.next(f)); // no more fields
}

TEST(ProtobufReaderTest, SingleStringField) {
    QByteArray data = TrezorProtobuf::encodeStringField(3, QStringLiteral("hello"));
    ProtobufReader reader(data);
    ProtobufReader::Field f;
    ASSERT_TRUE(reader.next(f));
    EXPECT_EQ(f.number, 3);
    EXPECT_EQ(f.wireType, 2);
    EXPECT_EQ(f.toString(), QStringLiteral("hello"));
}

TEST(ProtobufReaderTest, SingleBoolField) {
    QByteArray data = TrezorProtobuf::encodeBoolField(5, true);
    ProtobufReader reader(data);
    ProtobufReader::Field f;
    ASSERT_TRUE(reader.next(f));
    EXPECT_EQ(f.number, 5);
    EXPECT_TRUE(f.toBool());
}

TEST(ProtobufReaderTest, MultipleFieldsIterate) {
    QByteArray data;
    data.append(TrezorProtobuf::encodeUint32Field(1, 100));
    data.append(TrezorProtobuf::encodeStringField(2, QStringLiteral("test")));
    data.append(TrezorProtobuf::encodeBoolField(3, false));

    ProtobufReader reader(data);
    ProtobufReader::Field f;

    ASSERT_TRUE(reader.next(f));
    EXPECT_EQ(f.number, 1);
    EXPECT_EQ(f.toUInt64(), 100u);

    ASSERT_TRUE(reader.next(f));
    EXPECT_EQ(f.number, 2);
    EXPECT_EQ(f.toString(), QStringLiteral("test"));

    ASSERT_TRUE(reader.next(f));
    EXPECT_EQ(f.number, 3);
    EXPECT_FALSE(f.toBool());

    EXPECT_FALSE(reader.next(f));
}

TEST(ProtobufReaderTest, SkipsFixed32Fields) {
    // Manually encode: field 7 wire type 5 (fixed32) + 4 bytes, then field 1 varint 99
    QByteArray data;
    data.append(TrezorProtobuf::encodeTag(7, 5)); // field 7, wire type 5
    data.append(QByteArray(4, '\xAA'));           // 4 bytes of fixed32 data
    data.append(TrezorProtobuf::encodeUint32Field(1, 99));

    ProtobufReader reader(data);
    ProtobufReader::Field f;

    // Should skip field 7 (fixed32) and return field 1
    ASSERT_TRUE(reader.next(f));
    EXPECT_EQ(f.number, 1);
    EXPECT_EQ(f.toUInt64(), 99u);
}

TEST(ProtobufReaderTest, SkipsFixed64Fields) {
    QByteArray data;
    data.append(TrezorProtobuf::encodeTag(9, 1)); // field 9, wire type 1 (fixed64)
    data.append(QByteArray(8, '\xBB'));           // 8 bytes
    data.append(TrezorProtobuf::encodeStringField(2, QStringLiteral("after")));

    ProtobufReader reader(data);
    ProtobufReader::Field f;

    ASSERT_TRUE(reader.next(f));
    EXPECT_EQ(f.number, 2);
    EXPECT_EQ(f.toString(), QStringLiteral("after"));
}

TEST(ProtobufReaderTest, EmbeddedMessageViaToMessage) {
    // Build inner message: field 4 = "inner_value"
    QByteArray inner = TrezorProtobuf::encodeStringField(4, QStringLiteral("inner_value"));
    // Build outer: field 1 = inner (length-delimited, i.e. embedded message)
    QByteArray outer = TrezorProtobuf::encodeLengthDelimited(1, inner);

    ProtobufReader reader(outer);
    ProtobufReader::Field f;
    ASSERT_TRUE(reader.next(f));
    EXPECT_EQ(f.number, 1);
    EXPECT_EQ(f.wireType, 2);

    // Parse inner via toMessage()
    ProtobufReader innerReader = f.toMessage();
    ProtobufReader::Field inner_f;
    ASSERT_TRUE(innerReader.next(inner_f));
    EXPECT_EQ(inner_f.number, 4);
    EXPECT_EQ(inner_f.toString(), QStringLiteral("inner_value"));
}

TEST(ProtobufReaderTest, HighFieldNumbers) {
    // Field 501 requires multi-byte tag varint: (501 << 3) | 0 = 4008 → varint [0xA8, 0x1F]
    QByteArray data = TrezorProtobuf::encodeUint32Field(501, 7);
    ProtobufReader reader(data);
    ProtobufReader::Field f;
    ASSERT_TRUE(reader.next(f));
    EXPECT_EQ(f.number, 501);
    EXPECT_EQ(f.toUInt64(), 7u);
}

// ══════════════════════════════════════════════════════════════
// Message encoder tests
// ══════════════════════════════════════════════════════════════

TEST(EncoderTest, EncodeAddressN_TrezorPath) {
    QList<uint32_t> path = {
        44 | TrezorCrypto::kHardenedBit,
        501 | TrezorCrypto::kHardenedBit,
        0 | TrezorCrypto::kHardenedBit,
        0 | TrezorCrypto::kHardenedBit,
    };
    QByteArray encoded = TrezorProtobuf::encodeAddressN(path);

    // Verify each path component is field 1 varint
    ProtobufReader reader(encoded);
    ProtobufReader::Field f;
    for (int i = 0; i < 4; ++i) {
        ASSERT_TRUE(reader.next(f)) << "Missing path component " << i;
        EXPECT_EQ(f.number, TrezorFields::Common::kAddressN);
        EXPECT_EQ(f.wireType, 0);
        EXPECT_EQ(f.toUInt64(), path[i]);
    }
    EXPECT_FALSE(reader.next(f));
}

TEST(EncoderTest, EncodeSolanaGetPublicKey_NoDisplay) {
    QList<uint32_t> path = {44 | TrezorCrypto::kHardenedBit, 501 | TrezorCrypto::kHardenedBit};
    QByteArray encoded = TrezorProtobuf::encodeSolanaGetPublicKey(path, false);

    // Should contain 2 address_n fields, NO show_display field
    ProtobufReader reader(encoded);
    ProtobufReader::Field f;
    ASSERT_TRUE(reader.next(f));
    EXPECT_EQ(f.number, TrezorFields::Common::kAddressN);
    ASSERT_TRUE(reader.next(f));
    EXPECT_EQ(f.number, TrezorFields::Common::kAddressN);
    EXPECT_FALSE(reader.next(f)); // no more — show_display not emitted
}

TEST(EncoderTest, EncodeSolanaGetPublicKey_WithDisplay) {
    QList<uint32_t> path = {44 | TrezorCrypto::kHardenedBit};
    QByteArray encoded = TrezorProtobuf::encodeSolanaGetPublicKey(path, true);

    ProtobufReader reader(encoded);
    ProtobufReader::Field f;
    ASSERT_TRUE(reader.next(f)); // address_n
    EXPECT_EQ(f.number, TrezorFields::Common::kAddressN);
    ASSERT_TRUE(reader.next(f)); // show_display
    EXPECT_EQ(f.number, TrezorFields::Common::kShowDisplay);
    EXPECT_TRUE(f.toBool());
}

TEST(EncoderTest, EncodeSolanaSignTx_ContainsSerializedTx) {
    QList<uint32_t> path = {44 | TrezorCrypto::kHardenedBit, 501 | TrezorCrypto::kHardenedBit};
    QByteArray fakeTx(128, '\xAB');
    QByteArray encoded = TrezorProtobuf::encodeSolanaSignTx(path, fakeTx);

    ProtobufReader reader(encoded);
    ProtobufReader::Field f;

    // Skip address_n fields
    ASSERT_TRUE(reader.next(f));
    EXPECT_EQ(f.number, TrezorFields::Common::kAddressN);
    ASSERT_TRUE(reader.next(f));
    EXPECT_EQ(f.number, TrezorFields::Common::kAddressN);

    // serialized_tx
    ASSERT_TRUE(reader.next(f));
    EXPECT_EQ(f.number, TrezorFields::SolanaSignTx::kSerializedTx);
    EXPECT_EQ(f.toBytes(), fakeTx);
}

TEST(EncoderTest, EncodeGetPublicKey_WithCurve) {
    QList<uint32_t> path = {44 | TrezorCrypto::kHardenedBit, 501 | TrezorCrypto::kHardenedBit};
    QByteArray encoded = TrezorProtobuf::encodeGetPublicKey(path, QStringLiteral("ed25519"), false);

    ProtobufReader reader(encoded);
    ProtobufReader::Field f;

    // address_n fields
    ASSERT_TRUE(reader.next(f));
    EXPECT_EQ(f.number, TrezorFields::Common::kAddressN);
    ASSERT_TRUE(reader.next(f));
    EXPECT_EQ(f.number, TrezorFields::Common::kAddressN);

    // ecdsa_curve_name
    ASSERT_TRUE(reader.next(f));
    EXPECT_EQ(f.number, TrezorFields::GetPublicKey::kCurveName);
    EXPECT_EQ(f.toString(), QStringLiteral("ed25519"));
}

TEST(EncoderTest, EncodePassphraseAck_Empty) {
    QByteArray encoded = TrezorProtobuf::encodePassphraseAck();
    ProtobufReader reader(encoded);
    ProtobufReader::Field f;
    ASSERT_TRUE(reader.next(f));
    EXPECT_EQ(f.number, TrezorFields::PassphraseAck::kPassphrase);
    EXPECT_EQ(f.toString(), QString());
}

TEST(EncoderTest, EncodePassphraseAck_WithValue) {
    QByteArray encoded = TrezorProtobuf::encodePassphraseAck(QStringLiteral("secret"));
    ProtobufReader reader(encoded);
    ProtobufReader::Field f;
    ASSERT_TRUE(reader.next(f));
    EXPECT_EQ(f.number, TrezorFields::PassphraseAck::kPassphrase);
    EXPECT_EQ(f.toString(), QStringLiteral("secret"));
}

TEST(EncoderTest, EncodeInitializeIsEmpty) {
    EXPECT_TRUE(TrezorProtobuf::encodeInitialize().isEmpty());
}

TEST(EncoderTest, EncodeButtonAckIsEmpty) {
    EXPECT_TRUE(TrezorProtobuf::encodeButtonAck().isEmpty());
}

// ══════════════════════════════════════════════════════════════
// Message decoder tests — synthetic protobuf payloads
// ══════════════════════════════════════════════════════════════

// Helper: build a protobuf payload from scratch using the encoder functions
static QByteArray buildFeaturesPayload(const QString& model, const QString& label, bool initialized,
                                       bool passphrase) {
    QByteArray data;
    // Add some unknown fields before the ones we care about (to test skip logic)
    data.append(TrezorProtobuf::encodeUint32Field(1, 1)); // major_version (field 1)
    data.append(TrezorProtobuf::encodeUint32Field(2, 0)); // minor_version (field 2)
    data.append(TrezorProtobuf::encodeUint32Field(3, 0)); // patch_version (field 3)
    // Fields we decode
    data.append(TrezorProtobuf::encodeBoolField(TrezorFields::Features::kInitialized, initialized));
    data.append(
        TrezorProtobuf::encodeBoolField(TrezorFields::Features::kPassphraseProtection, passphrase));
    data.append(TrezorProtobuf::encodeStringField(TrezorFields::Features::kModel, model));
    data.append(TrezorProtobuf::encodeStringField(TrezorFields::Features::kLabel, label));
    return data;
}

TEST(DecoderTest, DecodeFeatures_AllFields) {
    QByteArray data =
        buildFeaturesPayload(QStringLiteral("T3T1"), QStringLiteral("My Trezor"), true, false);

    TrezorFeatures features = TrezorProtobuf::decodeFeatures(data);
    EXPECT_EQ(features.model, QStringLiteral("T3T1"));
    EXPECT_EQ(features.label, QStringLiteral("My Trezor"));
    EXPECT_TRUE(features.initialized);
    EXPECT_FALSE(features.passphraseProtection);
}

TEST(DecoderTest, DecodeFeatures_WithPassphrase) {
    QByteArray data = buildFeaturesPayload(QStringLiteral("1"), QStringLiteral(""), false, true);

    TrezorFeatures features = TrezorProtobuf::decodeFeatures(data);
    EXPECT_EQ(features.model, QStringLiteral("1"));
    EXPECT_EQ(features.label, QString());
    EXPECT_FALSE(features.initialized);
    EXPECT_TRUE(features.passphraseProtection);
}

TEST(DecoderTest, DecodeFeatures_EmptyPayload) {
    TrezorFeatures features = TrezorProtobuf::decodeFeatures(QByteArray());
    EXPECT_EQ(features.model, QString());
    EXPECT_FALSE(features.initialized);
}

TEST(DecoderTest, DecodeFailure) {
    QByteArray data;
    data.append(TrezorProtobuf::encodeUint32Field(TrezorFields::Failure::kCode, 9));
    data.append(TrezorProtobuf::encodeStringField(TrezorFields::Failure::kMessage,
                                                  QStringLiteral("Unexpected message")));

    TrezorFailure failure = TrezorProtobuf::decodeFailure(data);
    EXPECT_EQ(failure.code, 9);
    EXPECT_EQ(failure.message, QStringLiteral("Unexpected message"));
}

TEST(DecoderTest, DecodeFailure_EmptyPayload) {
    TrezorFailure failure = TrezorProtobuf::decodeFailure(QByteArray());
    EXPECT_EQ(failure.code, 0);
    EXPECT_EQ(failure.message, QString());
}

TEST(DecoderTest, DecodeSolanaPublicKey) {
    QByteArray pubkey = fromHex(kExpectedPubkeyHex);
    ASSERT_EQ(pubkey.size(), TrezorCrypto::kEd25519KeySize);

    // Build SolanaPublicKey protobuf: field 1 = pubkey bytes
    QByteArray data =
        TrezorProtobuf::encodeLengthDelimited(TrezorFields::SolanaPublicKey::kPublicKey, pubkey);

    QByteArray decoded = TrezorProtobuf::decodeSolanaPublicKey(data);
    EXPECT_EQ(decoded.toHex(), QByteArray(kExpectedPubkeyHex));
    EXPECT_EQ(decoded.size(), TrezorCrypto::kEd25519KeySize);
}

TEST(DecoderTest, DecodeSolanaPublicKey_EmptyPayload) {
    EXPECT_TRUE(TrezorProtobuf::decodeSolanaPublicKey(QByteArray()).isEmpty());
}

TEST(DecoderTest, DecodeSolanaAddress) {
    // Build SolanaAddress protobuf: field 1 = address string
    QByteArray data = TrezorProtobuf::encodeStringField(TrezorFields::SolanaAddress::kAddress,
                                                        QString::fromLatin1(kExpectedAddress));

    QString decoded = TrezorProtobuf::decodeSolanaAddress(data);
    EXPECT_EQ(decoded, QString::fromLatin1(kExpectedAddress));
}

TEST(DecoderTest, DecodeSolanaAddress_EmptyPayload) {
    EXPECT_TRUE(TrezorProtobuf::decodeSolanaAddress(QByteArray()).isEmpty());
}

TEST(DecoderTest, DecodeSolanaTxSignature) {
    QByteArray sig(TrezorCrypto::kEd25519SignatureSize, '\x42');
    QByteArray data =
        TrezorProtobuf::encodeLengthDelimited(TrezorFields::SolanaTxSignature::kSignature, sig);

    QByteArray decoded = TrezorProtobuf::decodeSolanaTxSignature(data);
    EXPECT_EQ(decoded, sig);
    EXPECT_EQ(decoded.size(), TrezorCrypto::kEd25519SignatureSize);
}

TEST(DecoderTest, DecodePublicKeyNode_Ed25519Padded) {
    // Build HDNodeType embedded message: field 4 = 0x00 + 32-byte pubkey (SLIP-0010 format)
    QByteArray pubkey = fromHex(kExpectedPubkeyHex);
    QByteArray paddedPubkey;
    paddedPubkey.append(static_cast<char>(TrezorCrypto::kEd25519PadByte));
    paddedPubkey.append(pubkey);
    ASSERT_EQ(paddedPubkey.size(), TrezorCrypto::kEd25519PaddedKeySize);

    // HDNodeType has other fields too — add some to test skipping
    QByteArray hdNode;
    hdNode.append(TrezorProtobuf::encodeUint32Field(1, 3));          // depth = 3
    hdNode.append(TrezorProtobuf::encodeUint32Field(2, 0));          // fingerprint
    hdNode.append(TrezorProtobuf::encodeUint32Field(3, 0x80000000)); // child_num
    hdNode.append(TrezorProtobuf::encodeLengthDelimited(TrezorFields::HDNodeType::kPublicKey,
                                                        paddedPubkey)); // public_key
    // field 5 = chain_code (32 bytes) — should be skipped
    hdNode.append(TrezorProtobuf::encodeLengthDelimited(5, QByteArray(32, '\xCC')));

    // Wrap in PublicKey message: field 1 = HDNodeType, field 2 = xpub string
    QByteArray data;
    data.append(TrezorProtobuf::encodeLengthDelimited(TrezorFields::PublicKey::kNode, hdNode));
    data.append(TrezorProtobuf::encodeStringField(2, QStringLiteral("xpub_goes_here")));

    QByteArray decoded = TrezorProtobuf::decodePublicKeyNode(data);
    EXPECT_EQ(decoded.toHex(), QByteArray(kExpectedPubkeyHex));
    EXPECT_EQ(decoded.size(), TrezorCrypto::kEd25519KeySize);
}

TEST(DecoderTest, DecodePublicKeyNode_Unpadded32Bytes) {
    // Some firmware returns 32 bytes directly (no 0x00 prefix)
    QByteArray pubkey = fromHex(kExpectedPubkeyHex);

    QByteArray hdNode;
    hdNode.append(
        TrezorProtobuf::encodeLengthDelimited(TrezorFields::HDNodeType::kPublicKey, pubkey));

    QByteArray data = TrezorProtobuf::encodeLengthDelimited(TrezorFields::PublicKey::kNode, hdNode);

    QByteArray decoded = TrezorProtobuf::decodePublicKeyNode(data);
    EXPECT_EQ(decoded.toHex(), QByteArray(kExpectedPubkeyHex));
}

TEST(DecoderTest, DecodePublicKeyNode_EmptyPayload) {
    EXPECT_TRUE(TrezorProtobuf::decodePublicKeyNode(QByteArray()).isEmpty());
}

// ══════════════════════════════════════════════════════════════
// Encoder → Decoder round-trip
// ══════════════════════════════════════════════════════════════

TEST(RoundTripTest, SolanaGetPublicKey_EncodeDecode) {
    // Encode a SolanaGetPublicKey request, then parse it back
    QList<uint32_t> path = TrezorProtobuf::parseBip44Path(QStringLiteral("m/44'/501'/0'/0'"));
    ASSERT_EQ(path.size(), 4);

    QByteArray encoded = TrezorProtobuf::encodeSolanaGetPublicKey(path, false);

    // Parse back and verify all 4 path components
    ProtobufReader reader(encoded);
    ProtobufReader::Field f;
    for (int i = 0; i < 4; ++i) {
        ASSERT_TRUE(reader.next(f));
        EXPECT_EQ(f.number, TrezorFields::Common::kAddressN);
        EXPECT_EQ(f.toUInt64(), path[i]);
    }
}

TEST(RoundTripTest, Failure_EncodeDecode) {
    // Build a failure, decode it, verify fields survive round-trip
    QByteArray data;
    data.append(TrezorProtobuf::encodeUint32Field(TrezorFields::Failure::kCode, 4));
    data.append(TrezorProtobuf::encodeStringField(TrezorFields::Failure::kMessage,
                                                  QStringLiteral("PIN cancelled")));

    TrezorFailure f = TrezorProtobuf::decodeFailure(data);
    EXPECT_EQ(f.code, 4);
    EXPECT_EQ(f.message, QStringLiteral("PIN cancelled"));
}

TEST(RoundTripTest, Features_SkipsUnknownFields) {
    // A real Features message has ~40 fields. We only decode 4.
    // Verify unknown fields are skipped without corrupting the ones we read.
    QByteArray data;
    // Many unknown fields before and between the ones we care about
    data.append(TrezorProtobuf::encodeStringField(4, QStringLiteral("2.6.3")));    // fw_version
    data.append(TrezorProtobuf::encodeBoolField(6, true));                         // pin_protection
    data.append(TrezorProtobuf::encodeLengthDelimited(9, QByteArray(32, '\x00'))); // device_id
    data.append(TrezorProtobuf::encodeBoolField(TrezorFields::Features::kInitialized, true));
    data.append(TrezorProtobuf::encodeStringField(14, QStringLiteral("en-US"))); // language
    data.append(
        TrezorProtobuf::encodeBoolField(TrezorFields::Features::kPassphraseProtection, true));
    data.append(TrezorProtobuf::encodeUint32Field(20, 2)); // backup_type
    data.append(
        TrezorProtobuf::encodeStringField(TrezorFields::Features::kModel, QStringLiteral("T2B1")));
    data.append(
        TrezorProtobuf::encodeStringField(22, QStringLiteral("internal_model"))); // random field 22
    data.append(TrezorProtobuf::encodeStringField(TrezorFields::Features::kLabel,
                                                  QStringLiteral("Safe 3")));

    TrezorFeatures features = TrezorProtobuf::decodeFeatures(data);
    EXPECT_EQ(features.model, QStringLiteral("T2B1"));
    EXPECT_EQ(features.label, QStringLiteral("Safe 3"));
    EXPECT_TRUE(features.initialized);
    EXPECT_TRUE(features.passphraseProtection);
}

// ══════════════════════════════════════════════════════════════
// Constants sanity checks
// ══════════════════════════════════════════════════════════════

TEST(ConstantsTest, MessageTypeIDs) {
    // Verify Solana message types are in the 900 range (not the old 700 range)
    EXPECT_EQ(TrezorMsg::SolanaGetPublicKey, 900);
    EXPECT_EQ(TrezorMsg::SolanaPublicKey, 901);
    EXPECT_EQ(TrezorMsg::SolanaGetAddress, 902);
    EXPECT_EQ(TrezorMsg::SolanaAddress, 903);
    EXPECT_EQ(TrezorMsg::SolanaSignTx, 904);
    EXPECT_EQ(TrezorMsg::SolanaTxSignature, 905);
}

TEST(ConstantsTest, CryptoSizes) {
    EXPECT_EQ(TrezorCrypto::kEd25519KeySize, 32);
    EXPECT_EQ(TrezorCrypto::kEd25519PaddedKeySize, 33);
    EXPECT_EQ(TrezorCrypto::kEd25519SignatureSize, 64);
    EXPECT_EQ(TrezorCrypto::kHardenedBit, 0x80000000u);
}

TEST(ConstantsTest, WireProtocol) {
    EXPECT_EQ(TrezorWire::kMarker, 0x3F);
    EXPECT_EQ(TrezorWire::kUsbPacketSize, 64);
    EXPECT_EQ(TrezorWire::kHidWriteSize, 65);
    EXPECT_EQ(TrezorWire::kPayloadPerPacket, 63);
    EXPECT_EQ(TrezorWire::kFirstPayload, 55);
}

// ── main ─────────────────────────────────────────────────────

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
