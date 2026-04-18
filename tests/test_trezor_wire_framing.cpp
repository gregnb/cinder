#include "MockTrezorTransport.h"
#include "crypto/plugin/trezor/TrezorProtobuf.h"
#include "crypto/plugin/trezor/TrezorTransport.h"

#include <gtest/gtest.h>

// ══════════════════════════════════════════════════════════════
// Wire framing: sendMessage via mock
// ══════════════════════════════════════════════════════════════

class WireFramingTest : public ::testing::Test {
  protected:
    MockTrezorTransport transport;

    void SetUp() override {
        transport.open(QStringLiteral("mock"));
        // Always enqueue a Features response so call() has something to read
        transport.enqueueResponse(TrezorMsg::Features, QByteArray());
    }
};

TEST_F(WireFramingTest, EmptyMessageProducesOnePacket) {
    transport.call(TrezorMsg::Initialize, QByteArray());

    auto packets = transport.writtenPackets();
    ASSERT_EQ(packets.size(), 1);
    EXPECT_EQ(packets[0].size(), TrezorWire::kUsbPacketSize);
}

TEST_F(WireFramingTest, HeaderHasCorrectMarkerAndMagic) {
    transport.call(TrezorMsg::Initialize, QByteArray());

    auto packets = transport.writtenPackets();
    ASSERT_GE(packets[0].size(), 9);
    EXPECT_EQ(static_cast<uint8_t>(packets[0][0]), TrezorWire::kMarker);
    EXPECT_EQ(packets[0][1], TrezorWire::kHeaderMagic0);
    EXPECT_EQ(packets[0][2], TrezorWire::kHeaderMagic1);
}

TEST_F(WireFramingTest, HeaderEncodesMessageType) {
    transport.call(TrezorMsg::SolanaGetPublicKey, QByteArray());

    auto h = transport.parseFirstWrittenHeader();
    EXPECT_EQ(h.msgType, TrezorMsg::SolanaGetPublicKey);
    EXPECT_EQ(h.msgType, 900); // sanity: not the old 700
}

TEST_F(WireFramingTest, HeaderEncodesPayloadLength) {
    QByteArray payload(42, '\xAA');
    transport.call(TrezorMsg::SolanaSignTx, payload);

    auto h = transport.parseFirstWrittenHeader();
    EXPECT_EQ(h.dataLen, 42u);
}

TEST_F(WireFramingTest, SmallPayloadFitsInOnePacket) {
    QByteArray payload(10, '\xBB');
    transport.call(TrezorMsg::SolanaGetPublicKey, payload);

    auto packets = transport.writtenPackets();
    EXPECT_EQ(packets.size(), 1);

    // Payload should be embedded in the first packet after the header
    QByteArray reassembled = transport.reassembleWrittenPayload();
    // reassembleWrittenPayload strips padding, so trim trailing zeros
    reassembled.truncate(payload.size());
    EXPECT_EQ(reassembled, payload);
}

TEST_F(WireFramingTest, LargePayloadSpansMultiplePackets) {
    // First packet can hold 55 bytes of payload (64 - marker - header)
    // Each continuation can hold 63 bytes (64 - marker)
    QByteArray payload(200, '\xCC');
    transport.call(TrezorMsg::SolanaSignTx, payload);

    auto packets = transport.writtenPackets();
    // 200 bytes: 55 in first packet, then ceil(145/63) = 3 continuation packets = 4 total
    EXPECT_EQ(packets.size(), 4);

    // All packets should be exactly kUsbPacketSize
    for (const auto& pkt : packets) {
        EXPECT_EQ(pkt.size(), TrezorWire::kUsbPacketSize);
    }

    // Verify payload integrity
    QByteArray reassembled = transport.reassembleWrittenPayload();
    reassembled.truncate(payload.size());
    EXPECT_EQ(reassembled, payload);
}

TEST_F(WireFramingTest, ExactlyFirstPayloadSizeIsOnePacket) {
    // 55 bytes of payload should fit in exactly one packet
    QByteArray payload(TrezorWire::kFirstPayload, '\xDD');
    transport.call(TrezorMsg::SolanaGetPublicKey, payload);

    EXPECT_EQ(transport.writtenPackets().size(), 1);
}

TEST_F(WireFramingTest, FirstPayloadPlusOneNeedsTwoPackets) {
    QByteArray payload(TrezorWire::kFirstPayload + 1, '\xEE');
    transport.call(TrezorMsg::SolanaGetPublicKey, payload);

    EXPECT_EQ(transport.writtenPackets().size(), 2);
}

// ══════════════════════════════════════════════════════════════
// Wire framing: readMessage via mock
// ══════════════════════════════════════════════════════════════

TEST(ReadMessageTest, DecodesEmptyResponse) {
    MockTrezorTransport transport;
    transport.open(QStringLiteral("mock"));
    transport.enqueueResponse(TrezorMsg::Features, QByteArray());

    TrezorResponse resp = transport.call(TrezorMsg::Initialize, QByteArray());
    EXPECT_TRUE(resp.isValid());
    EXPECT_EQ(resp.msgType, TrezorMsg::Features);
    EXPECT_TRUE(resp.data.isEmpty());
}

TEST(ReadMessageTest, DecodesSmallPayload) {
    MockTrezorTransport transport;
    transport.open(QStringLiteral("mock"));

    QByteArray pubkey(32, '\x42');
    QByteArray protobuf =
        TrezorProtobuf::encodeLengthDelimited(TrezorFields::SolanaPublicKey::kPublicKey, pubkey);
    transport.enqueueResponse(TrezorMsg::SolanaPublicKey, protobuf);

    TrezorResponse resp = transport.call(TrezorMsg::SolanaGetPublicKey, QByteArray());
    EXPECT_EQ(resp.msgType, TrezorMsg::SolanaPublicKey);

    QByteArray decoded = TrezorProtobuf::decodeSolanaPublicKey(resp.data);
    EXPECT_EQ(decoded, pubkey);
}

TEST(ReadMessageTest, ReassemblesMultiPacketResponse) {
    MockTrezorTransport transport;
    transport.open(QStringLiteral("mock"));

    // Create a large protobuf payload that will span multiple read packets
    QByteArray bigPayload(200, '\xAA');
    transport.enqueueResponse(TrezorMsg::Features, bigPayload);

    TrezorResponse resp = transport.call(TrezorMsg::Initialize, QByteArray());
    EXPECT_EQ(resp.msgType, TrezorMsg::Features);
    EXPECT_EQ(resp.data, bigPayload);
}

TEST(ReadMessageTest, FailsOnNoQueuedResponse) {
    MockTrezorTransport transport;
    transport.open(QStringLiteral("mock"));
    // Don't enqueue anything

    TrezorResponse resp = transport.call(TrezorMsg::Initialize, QByteArray());
    EXPECT_FALSE(resp.isValid());
}

TEST(ReadMessageTest, FailsWhenClosed) {
    MockTrezorTransport transport;
    // Don't open
    TrezorResponse resp = transport.call(TrezorMsg::Initialize, QByteArray());
    EXPECT_FALSE(resp.isValid());
    EXPECT_EQ(transport.lastError(), QStringLiteral("Device not open"));
}

// ══════════════════════════════════════════════════════════════
// driveInteraction helper
// ══════════════════════════════════════════════════════════════

TEST(DriveInteractionTest, PassesThroughNonInteractiveResponse) {
    MockTrezorTransport transport;
    transport.open(QStringLiteral("mock"));

    // A Failure response should pass straight through
    QByteArray failData;
    failData.append(TrezorProtobuf::encodeUint32Field(TrezorFields::Failure::kCode, 9));
    TrezorResponse initial;
    initial.msgType = TrezorMsg::Failure;
    initial.data = failData;

    TrezorResponse result = TrezorProtobuf::driveInteraction(&transport, std::move(initial));
    EXPECT_EQ(result.msgType, TrezorMsg::Failure);
    EXPECT_EQ(TrezorProtobuf::decodeFailure(result.data).code, 9);
}

TEST(DriveInteractionTest, PassesThroughFinalResponse) {
    MockTrezorTransport transport;
    transport.open(QStringLiteral("mock"));

    QByteArray pubkey(32, '\x42');
    TrezorResponse initial;
    initial.msgType = TrezorMsg::SolanaPublicKey;
    initial.data =
        TrezorProtobuf::encodeLengthDelimited(TrezorFields::SolanaPublicKey::kPublicKey, pubkey);

    TrezorResponse result = TrezorProtobuf::driveInteraction(&transport, std::move(initial));
    EXPECT_EQ(result.msgType, TrezorMsg::SolanaPublicKey);
}

TEST(DriveInteractionTest, HandlesButtonRequest) {
    MockTrezorTransport transport;
    transport.open(QStringLiteral("mock"));

    // After we send ButtonAck, the device will respond with the final result
    QByteArray pubkey(32, '\x42');
    QByteArray pubkeyProto =
        TrezorProtobuf::encodeLengthDelimited(TrezorFields::SolanaPublicKey::kPublicKey, pubkey);
    transport.enqueueResponse(TrezorMsg::SolanaPublicKey, pubkeyProto);

    // Start with a ButtonRequest
    TrezorResponse initial;
    initial.msgType = TrezorMsg::ButtonRequest;

    TrezorResponse result = TrezorProtobuf::driveInteraction(&transport, std::move(initial));
    EXPECT_EQ(result.msgType, TrezorMsg::SolanaPublicKey);

    // Verify a ButtonAck was sent
    auto h = transport.parseFirstWrittenHeader();
    EXPECT_EQ(h.msgType, TrezorMsg::ButtonAck);
}

TEST(DriveInteractionTest, HandlesPassphraseRequest) {
    MockTrezorTransport transport;
    transport.open(QStringLiteral("mock"));

    // After PassphraseAck, device returns the final result
    transport.enqueueResponse(TrezorMsg::SolanaPublicKey, QByteArray());

    TrezorResponse initial;
    initial.msgType = TrezorMsg::PassphraseRequest;

    TrezorResponse result = TrezorProtobuf::driveInteraction(&transport, std::move(initial));
    EXPECT_EQ(result.msgType, TrezorMsg::SolanaPublicKey);

    auto h = transport.parseFirstWrittenHeader();
    EXPECT_EQ(h.msgType, TrezorMsg::PassphraseAck);
}

TEST(DriveInteractionTest, HandlesMultipleButtonRequests) {
    MockTrezorTransport transport;
    transport.open(QStringLiteral("mock"));

    // Device sends ButtonRequest twice before final response
    transport.enqueueResponse(TrezorMsg::ButtonRequest, QByteArray());
    transport.enqueueResponse(TrezorMsg::SolanaPublicKey, QByteArray());

    TrezorResponse initial;
    initial.msgType = TrezorMsg::ButtonRequest;

    TrezorResponse result = TrezorProtobuf::driveInteraction(&transport, std::move(initial));
    EXPECT_EQ(result.msgType, TrezorMsg::SolanaPublicKey);

    // Should have sent 2 ButtonAcks (one for initial, one for second ButtonRequest)
    EXPECT_EQ(transport.writtenPackets().size(), 2);
}

TEST(DriveInteractionTest, HandlesMixedButtonAndPassphrase) {
    MockTrezorTransport transport;
    transport.open(QStringLiteral("mock"));

    // ButtonRequest → PassphraseRequest → final
    transport.enqueueResponse(TrezorMsg::PassphraseRequest, QByteArray());
    transport.enqueueResponse(TrezorMsg::SolanaPublicKey, QByteArray());

    TrezorResponse initial;
    initial.msgType = TrezorMsg::ButtonRequest;

    TrezorResponse result = TrezorProtobuf::driveInteraction(&transport, std::move(initial));
    EXPECT_EQ(result.msgType, TrezorMsg::SolanaPublicKey);
    EXPECT_EQ(transport.writtenPackets().size(), 2);
}

// ══════════════════════════════════════════════════════════════
// Full protocol round-trip via mock
// ══════════════════════════════════════════════════════════════

TEST(ProtocolRoundTripTest, InitializeAndGetPublicKey) {
    MockTrezorTransport transport;
    transport.open(QStringLiteral("mock"));

    // 1. Initialize → Features
    QByteArray featuresProto =
        TrezorProtobuf::encodeStringField(TrezorFields::Features::kModel, QStringLiteral("T3T1"));
    featuresProto.append(
        TrezorProtobuf::encodeBoolField(TrezorFields::Features::kInitialized, true));
    transport.enqueueResponse(TrezorMsg::Features, featuresProto);

    TrezorResponse resp = transport.call(TrezorMsg::Initialize, TrezorProtobuf::encodeInitialize(),
                                         TrezorTimeout::kInit);
    ASSERT_EQ(resp.msgType, TrezorMsg::Features);
    TrezorFeatures features = TrezorProtobuf::decodeFeatures(resp.data);
    EXPECT_EQ(features.model, QStringLiteral("T3T1"));
    EXPECT_TRUE(features.initialized);

    // 2. SolanaGetPublicKey → ButtonRequest → SolanaPublicKey
    transport.clearWrittenPackets();
    QByteArray pubkey =
        QByteArray::fromHex("f276122d477c6df38156d8763c6a4f95666984300bdbc47d7833a9089b58bfad");
    QByteArray pubkeyProto =
        TrezorProtobuf::encodeLengthDelimited(TrezorFields::SolanaPublicKey::kPublicKey, pubkey);

    transport.enqueueResponse(TrezorMsg::ButtonRequest, QByteArray());
    transport.enqueueResponse(TrezorMsg::SolanaPublicKey, pubkeyProto);

    QList<uint32_t> path = TrezorProtobuf::parseBip44Path(QStringLiteral("m/44'/501'/0'/0'"));
    resp = transport.call(TrezorMsg::SolanaGetPublicKey,
                          TrezorProtobuf::encodeSolanaGetPublicKey(path), TrezorTimeout::kDefault);

    // First response is ButtonRequest — drive through it
    ASSERT_EQ(resp.msgType, TrezorMsg::ButtonRequest);
    resp = TrezorProtobuf::driveInteraction(&transport, std::move(resp));

    ASSERT_EQ(resp.msgType, TrezorMsg::SolanaPublicKey);
    QByteArray decodedPubkey = TrezorProtobuf::decodeSolanaPublicKey(resp.data);
    EXPECT_EQ(decodedPubkey.toHex(),
              QByteArray("f276122d477c6df38156d8763c6a4f95666984300bdbc47d7833a9089b58bfad"));
}

// ── main ─────────────────────────────────────────────────────

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
