// Standalone test: verify TrezorFeatures decoding against real protobuf field numbers
// from trezor-firmware/common/protob/messages-management.proto

#include "crypto/plugin/trezor/TrezorProtobuf.h"
#include <gtest/gtest.h>

// Helper: encode a varint field (wire type 0) into raw protobuf bytes
static QByteArray encodeVarintField(int fieldNumber, uint64_t value) {
    QByteArray result;
    // Tag = (fieldNumber << 3) | wireType(0)
    uint64_t tag = static_cast<uint64_t>((fieldNumber << 3) | 0);
    while (tag > 0x7F) {
        result.append(static_cast<char>((tag & 0x7F) | 0x80));
        tag >>= 7;
    }
    result.append(static_cast<char>(tag & 0x7F));
    // Value
    while (value > 0x7F) {
        result.append(static_cast<char>((value & 0x7F) | 0x80));
        value >>= 7;
    }
    result.append(static_cast<char>(value & 0x7F));
    return result;
}

// Helper: encode a length-delimited field (wire type 2) into raw protobuf bytes
static QByteArray encodeLenField(int fieldNumber, const QByteArray& data) {
    QByteArray result;
    uint64_t tag = static_cast<uint64_t>((fieldNumber << 3) | 2);
    while (tag > 0x7F) {
        result.append(static_cast<char>((tag & 0x7F) | 0x80));
        tag >>= 7;
    }
    result.append(static_cast<char>(tag & 0x7F));
    uint64_t len = static_cast<uint64_t>(data.size());
    while (len > 0x7F) {
        result.append(static_cast<char>((len & 0x7F) | 0x80));
        len >>= 7;
    }
    result.append(static_cast<char>(len & 0x7F));
    result.append(data);
    return result;
}

// ── Verify field number constants match trezor-firmware proto ──

TEST(FeaturesFieldNumbers, MatchTrezorFirmwareProto) {
    // From: trezor-firmware/common/protob/messages-management.proto
    // message Features { ... }
    EXPECT_EQ(TrezorFields::Features::kMajorVersion, 2);
    EXPECT_EQ(TrezorFields::Features::kMinorVersion, 3);
    EXPECT_EQ(TrezorFields::Features::kPatchVersion, 4);
    EXPECT_EQ(TrezorFields::Features::kDeviceId, 6);
    EXPECT_EQ(TrezorFields::Features::kPinProtection, 7);
    EXPECT_EQ(TrezorFields::Features::kPassphraseProtection, 8);
    EXPECT_EQ(TrezorFields::Features::kLabel, 10);
    EXPECT_EQ(TrezorFields::Features::kInitialized, 12);
    EXPECT_EQ(TrezorFields::Features::kUnlocked, 16);
    EXPECT_EQ(TrezorFields::Features::kModel, 21);
    EXPECT_EQ(TrezorFields::Features::kInternalModel, 44);
}

// ── Build a realistic Features protobuf and decode it ──

TEST(FeaturesDecoder, DecodesAllFields) {
    // Simulate a Features response like a real Trezor Safe 3 would send
    QByteArray data;
    data.append(encodeVarintField(2, 2));           // major_version = 2
    data.append(encodeVarintField(3, 8));           // minor_version = 8
    data.append(encodeVarintField(4, 1));           // patch_version = 1
    data.append(encodeLenField(6, "ABCDEF123456")); // device_id
    data.append(encodeVarintField(7, 1));           // pin_protection = true
    data.append(encodeVarintField(8, 0));           // passphrase_protection = false
    data.append(encodeLenField(10, "My Trezor"));   // label
    data.append(encodeVarintField(12, 1));          // initialized = true
    data.append(encodeVarintField(16, 1));          // unlocked = true
    data.append(encodeLenField(21, "Safe 3"));      // model
    data.append(encodeLenField(44, "T2B1"));        // internal_model

    TrezorFeatures f = TrezorProtobuf::decodeFeatures(data);

    EXPECT_EQ(f.majorVersion, 2);
    EXPECT_EQ(f.minorVersion, 8);
    EXPECT_EQ(f.patchVersion, 1);
    EXPECT_EQ(f.firmwareVersionString(), "2.8.1");
    EXPECT_EQ(f.deviceId, "ABCDEF123456");
    EXPECT_TRUE(f.pinProtection);
    EXPECT_FALSE(f.passphraseProtection);
    EXPECT_EQ(f.label, "My Trezor");
    EXPECT_TRUE(f.initialized);
    EXPECT_TRUE(f.unlocked);
    EXPECT_EQ(f.model, "Safe 3");
    EXPECT_EQ(f.internalModel, "T2B1");
}

TEST(FeaturesDecoder, PinProtectionTrue) {
    // Minimal message with just pin_protection = true at field 7
    QByteArray data = encodeVarintField(7, 1);
    TrezorFeatures f = TrezorProtobuf::decodeFeatures(data);
    EXPECT_TRUE(f.pinProtection);
}

TEST(FeaturesDecoder, PinProtectionFalse) {
    // pin_protection = false at field 7
    QByteArray data = encodeVarintField(7, 0);
    TrezorFeatures f = TrezorProtobuf::decodeFeatures(data);
    EXPECT_FALSE(f.pinProtection);
}

TEST(FeaturesDecoder, PassphraseProtectionTrue) {
    // passphrase_protection = true at field 8
    QByteArray data = encodeVarintField(8, 1);
    TrezorFeatures f = TrezorProtobuf::decodeFeatures(data);
    EXPECT_TRUE(f.passphraseProtection);
}

TEST(FeaturesDecoder, LabelAtField10) {
    QByteArray data = encodeLenField(10, "Satoshi's Wallet");
    TrezorFeatures f = TrezorProtobuf::decodeFeatures(data);
    EXPECT_EQ(f.label, "Satoshi's Wallet");
}

TEST(FeaturesDecoder, DeviceIdAtField6) {
    QByteArray data = encodeLenField(6, "XYZ789");
    TrezorFeatures f = TrezorProtobuf::decodeFeatures(data);
    EXPECT_EQ(f.deviceId, "XYZ789");
}

TEST(FeaturesDecoder, UnlockedAtField16) {
    QByteArray data = encodeVarintField(16, 1);
    TrezorFeatures f = TrezorProtobuf::decodeFeatures(data);
    EXPECT_TRUE(f.unlocked);
}

TEST(FeaturesDecoder, InternalModelAtField44) {
    QByteArray data = encodeLenField(44, "T3T1");
    TrezorFeatures f = TrezorProtobuf::decodeFeatures(data);
    EXPECT_EQ(f.internalModel, "T3T1");
}

TEST(FeaturesDecoder, SkipsUnknownFields) {
    // Include fields we don't care about (field 1 = vendor, field 9 = language)
    QByteArray data;
    data.append(encodeLenField(1, "SatoshiLabs")); // vendor (unknown to our decoder)
    data.append(encodeLenField(9, "en-US"));       // language (unknown)
    data.append(encodeVarintField(7, 1));          // pin_protection = true
    data.append(encodeLenField(21, "Safe 5"));     // model

    TrezorFeatures f = TrezorProtobuf::decodeFeatures(data);
    EXPECT_TRUE(f.pinProtection);
    EXPECT_EQ(f.model, "Safe 5");
}

TEST(FeaturesDecoder, EmptyDataReturnsDefaults) {
    TrezorFeatures f = TrezorProtobuf::decodeFeatures({});
    EXPECT_EQ(f.majorVersion, 0);
    EXPECT_EQ(f.minorVersion, 0);
    EXPECT_EQ(f.patchVersion, 0);
    EXPECT_TRUE(f.model.isEmpty());
    EXPECT_TRUE(f.label.isEmpty());
    EXPECT_TRUE(f.deviceId.isEmpty());
    EXPECT_FALSE(f.pinProtection);
    EXPECT_FALSE(f.passphraseProtection);
    EXPECT_FALSE(f.initialized);
    EXPECT_FALSE(f.unlocked);
}

// ── Verify the old wrong field numbers DON'T decode ──

TEST(FeaturesDecoder, OldWrongFieldNumbersDontDecode) {
    // These were the WRONG field numbers previously used:
    // pin_protection was at 9 (wrong, correct is 7)
    // passphrase_protection was at 17 (wrong, correct is 8)
    // label was at 24 (wrong, correct is 10)
    // device_id was at 15 (wrong, correct is 6)
    QByteArray data;
    data.append(encodeVarintField(9, 1));        // WRONG: pin at field 9
    data.append(encodeVarintField(17, 1));       // WRONG: passphrase at field 17
    data.append(encodeLenField(24, "BadLabel")); // WRONG: label at field 24
    data.append(encodeLenField(15, "BadId"));    // WRONG: device_id at field 15

    TrezorFeatures f = TrezorProtobuf::decodeFeatures(data);
    // None of these should decode with the corrected field numbers
    EXPECT_FALSE(f.pinProtection);
    EXPECT_FALSE(f.passphraseProtection);
    EXPECT_TRUE(f.label.isEmpty());
    EXPECT_TRUE(f.deviceId.isEmpty());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
