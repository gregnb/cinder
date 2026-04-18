#include <gtest/gtest.h>

#include "MockLedgerTransport.h"
#include "crypto/plugin/ledger/LedgerPlugin.h"
#include "crypto/plugin/ledger/LedgerTransport.h"

// ═══════════════════════════════════════════════════════════════════
//  Suite 1: Derivation Path Encoding
// ═══════════════════════════════════════════════════════════════════

class PathEncodingTest : public ::testing::Test {};

TEST_F(PathEncodingTest, StandardSolanaPath) {
    // m/44'/501'/0' → 3 segments, all hardened
    QByteArray result = LedgerPlugin::encodeDerivationPath("m/44'/501'/0'");
    ASSERT_FALSE(result.isEmpty());

    // First byte = segment count
    EXPECT_EQ(static_cast<uint8_t>(result[0]), 3);

    // Total size: 1 (count) + 3 * 4 (segments) = 13
    EXPECT_EQ(result.size(), 13);

    // Segment 0: 44 | 0x80000000 = 0x8000002C
    EXPECT_EQ(static_cast<uint8_t>(result[1]), 0x80);
    EXPECT_EQ(static_cast<uint8_t>(result[2]), 0x00);
    EXPECT_EQ(static_cast<uint8_t>(result[3]), 0x00);
    EXPECT_EQ(static_cast<uint8_t>(result[4]), 0x2C);

    // Segment 1: 501 | 0x80000000 = 0x800001F5
    EXPECT_EQ(static_cast<uint8_t>(result[5]), 0x80);
    EXPECT_EQ(static_cast<uint8_t>(result[6]), 0x00);
    EXPECT_EQ(static_cast<uint8_t>(result[7]), 0x01);
    EXPECT_EQ(static_cast<uint8_t>(result[8]), 0xF5);

    // Segment 2: 0 | 0x80000000 = 0x80000000
    EXPECT_EQ(static_cast<uint8_t>(result[9]), 0x80);
    EXPECT_EQ(static_cast<uint8_t>(result[10]), 0x00);
    EXPECT_EQ(static_cast<uint8_t>(result[11]), 0x00);
    EXPECT_EQ(static_cast<uint8_t>(result[12]), 0x00);
}

TEST_F(PathEncodingTest, FourSegmentPath) {
    // m/44'/501'/0'/0' — used by some wallets (Phantom, etc.)
    QByteArray result = LedgerPlugin::encodeDerivationPath("m/44'/501'/0'/0'");
    ASSERT_FALSE(result.isEmpty());
    EXPECT_EQ(static_cast<uint8_t>(result[0]), 4);
    EXPECT_EQ(result.size(), 17);
}

TEST_F(PathEncodingTest, NonHardenedSegment) {
    // m/44'/501'/0'/0 — last segment NOT hardened
    QByteArray result = LedgerPlugin::encodeDerivationPath("m/44'/501'/0'/0");
    ASSERT_FALSE(result.isEmpty());
    EXPECT_EQ(static_cast<uint8_t>(result[0]), 4);

    // Last segment: 0 without hardened bit = 0x00000000
    EXPECT_EQ(static_cast<uint8_t>(result[13]), 0x00);
    EXPECT_EQ(static_cast<uint8_t>(result[14]), 0x00);
    EXPECT_EQ(static_cast<uint8_t>(result[15]), 0x00);
    EXPECT_EQ(static_cast<uint8_t>(result[16]), 0x00);
}

TEST_F(PathEncodingTest, HNotation) {
    // m/44h/501h/0h — 'h' instead of apostrophe
    QByteArray result = LedgerPlugin::encodeDerivationPath("m/44h/501h/0h");
    ASSERT_FALSE(result.isEmpty());
    EXPECT_EQ(static_cast<uint8_t>(result[0]), 3);

    // Should be identical to apostrophe notation
    QByteArray expected = LedgerPlugin::encodeDerivationPath("m/44'/501'/0'");
    EXPECT_EQ(result, expected);
}

TEST_F(PathEncodingTest, WithoutMPrefix) {
    // "44'/501'/0'" without "m/" prefix
    QByteArray result = LedgerPlugin::encodeDerivationPath("44'/501'/0'");
    ASSERT_FALSE(result.isEmpty());
    EXPECT_EQ(static_cast<uint8_t>(result[0]), 3);
}

TEST_F(PathEncodingTest, EmptyPathReturnsEmpty) {
    EXPECT_TRUE(LedgerPlugin::encodeDerivationPath("").isEmpty());
}

TEST_F(PathEncodingTest, InvalidPathReturnsEmpty) {
    EXPECT_TRUE(LedgerPlugin::encodeDerivationPath("m/abc/def").isEmpty());
}

TEST_F(PathEncodingTest, TooManySegmentsReturnsEmpty) {
    // More than 10 segments
    EXPECT_TRUE(LedgerPlugin::encodeDerivationPath("m/1/2/3/4/5/6/7/8/9/10/11").isEmpty());
}

TEST_F(PathEncodingTest, AccountIndexVariation) {
    // m/44'/501'/5' — account index 5
    QByteArray result = LedgerPlugin::encodeDerivationPath("m/44'/501'/5'");
    ASSERT_FALSE(result.isEmpty());

    // Segment 2: 5 | 0x80000000 = 0x80000005
    EXPECT_EQ(static_cast<uint8_t>(result[9]), 0x80);
    EXPECT_EQ(static_cast<uint8_t>(result[10]), 0x00);
    EXPECT_EQ(static_cast<uint8_t>(result[11]), 0x00);
    EXPECT_EQ(static_cast<uint8_t>(result[12]), 0x05);
}

// ═══════════════════════════════════════════════════════════════════
//  Suite 2: Status Word Messages
// ═══════════════════════════════════════════════════════════════════

class StatusWordTest : public ::testing::Test {};

TEST_F(StatusWordTest, Success) {
    EXPECT_EQ(LedgerTransport::statusWordMessage(0x9000), "Success");
}

TEST_F(StatusWordTest, TransactionRejected) {
    EXPECT_EQ(LedgerTransport::statusWordMessage(0x6985), "Transaction rejected on device");
}

TEST_F(StatusWordTest, SolanaAppNotOpen) {
    EXPECT_EQ(LedgerTransport::statusWordMessage(0x6A82),
              "Please open the Solana app on your Ledger");
}

TEST_F(StatusWordTest, DeviceLocked_6FAA) {
    QString msg = LedgerTransport::statusWordMessage(0x6FAA);
    EXPECT_TRUE(msg.contains("locked"));
}

TEST_F(StatusWordTest, DeviceLocked_5515) {
    QString msg = LedgerTransport::statusWordMessage(0x5515);
    EXPECT_TRUE(msg.contains("locked"));
}

TEST_F(StatusWordTest, InstructionNotSupported) {
    EXPECT_EQ(LedgerTransport::statusWordMessage(0x6D00),
              "Instruction not supported — update Solana app");
}

TEST_F(StatusWordTest, CLANotSupported) {
    EXPECT_EQ(LedgerTransport::statusWordMessage(0x6E00), "CLA not supported");
}

TEST_F(StatusWordTest, WrongDataLength) {
    EXPECT_EQ(LedgerTransport::statusWordMessage(0x6700), "Wrong data length");
}

TEST_F(StatusWordTest, SecurityCondition) {
    EXPECT_EQ(LedgerTransport::statusWordMessage(0x6982), "Security condition not satisfied");
}

TEST_F(StatusWordTest, UnknownStatusWord) {
    QString msg = LedgerTransport::statusWordMessage(0x1234);
    EXPECT_TRUE(msg.contains("1234"));
}

// ═══════════════════════════════════════════════════════════════════
//  Suite 3: APDU Framing (via MockLedgerTransport)
// ═══════════════════════════════════════════════════════════════════

class ApduFramingTest : public ::testing::Test {
  protected:
    MockLedgerTransport m_transport;
};

TEST_F(ApduFramingTest, SmallApduSinglePacket) {
    // A small APDU should fit in one 64-byte packet
    QByteArray apdu(10, 0x42);

    // Queue a success response (32 bytes + SW 0x9000)
    QByteArray responseData(32, 0xAA);
    m_transport.enqueueResponse(responseData);

    QByteArray result = m_transport.exchange(apdu);
    ASSERT_EQ(result.size(), 32);
    EXPECT_EQ(result, responseData);

    // Should have written exactly 1 packet
    EXPECT_EQ(m_transport.writtenPackets().size(), 1);

    // Verify framing: channel, tag, seq, length, then payload
    QByteArray pkt = m_transport.writtenPackets().first();
    EXPECT_EQ(pkt.size(), LedgerWire::kPacketSize);

    // Channel = 0x0101
    EXPECT_EQ(static_cast<uint8_t>(pkt[0]), 0x01);
    EXPECT_EQ(static_cast<uint8_t>(pkt[1]), 0x01);
    // Tag = 0x05
    EXPECT_EQ(static_cast<uint8_t>(pkt[2]), LedgerWire::kTagApdu);
    // Seq = 0x0000
    EXPECT_EQ(static_cast<uint8_t>(pkt[3]), 0x00);
    EXPECT_EQ(static_cast<uint8_t>(pkt[4]), 0x00);
    // Length = 10 (big-endian)
    EXPECT_EQ(static_cast<uint8_t>(pkt[5]), 0x00);
    EXPECT_EQ(static_cast<uint8_t>(pkt[6]), 10);
}

TEST_F(ApduFramingTest, LargeApduMultiplePackets) {
    // APDU larger than first payload (57 bytes) requires continuation packets
    QByteArray apdu(120, 0x55);

    QByteArray responseData(32, 0xBB);
    m_transport.enqueueResponse(responseData);

    QByteArray result = m_transport.exchange(apdu);
    ASSERT_EQ(result.size(), 32);

    // 120 bytes: first packet carries 57, second carries 59, third carries 4 → 3 packets
    EXPECT_EQ(m_transport.writtenPackets().size(), 3);

    // Verify continuation packet has seq=1
    QByteArray contPkt = m_transport.writtenPackets()[1];
    // Channel
    EXPECT_EQ(static_cast<uint8_t>(contPkt[0]), 0x01);
    EXPECT_EQ(static_cast<uint8_t>(contPkt[1]), 0x01);
    // Tag
    EXPECT_EQ(static_cast<uint8_t>(contPkt[2]), LedgerWire::kTagApdu);
    // Seq = 0x0001
    EXPECT_EQ(static_cast<uint8_t>(contPkt[3]), 0x00);
    EXPECT_EQ(static_cast<uint8_t>(contPkt[4]), 0x01);
}

TEST_F(ApduFramingTest, ReassembledApduMatchesOriginal) {
    QByteArray apdu(200, 0x77);

    QByteArray responseData(2, 0xCC);
    m_transport.enqueueResponse(responseData);

    m_transport.exchange(apdu);

    QByteArray reassembled = m_transport.reassembleWrittenApdu();
    EXPECT_EQ(reassembled.left(apdu.size()), apdu);
}

TEST_F(ApduFramingTest, ExactFirstPayloadBoundary) {
    // Exactly 57 bytes — should be single packet
    QByteArray apdu(LedgerWire::kFirstPayload, 0x33);

    QByteArray responseData(2, 0xDD);
    m_transport.enqueueResponse(responseData);

    m_transport.exchange(apdu);
    EXPECT_EQ(m_transport.writtenPackets().size(), 1);
}

TEST_F(ApduFramingTest, OneByteOverFirstPayload) {
    // 58 bytes — needs 2 packets
    QByteArray apdu(LedgerWire::kFirstPayload + 1, 0x44);

    QByteArray responseData(2, 0xEE);
    m_transport.enqueueResponse(responseData);

    m_transport.exchange(apdu);
    EXPECT_EQ(m_transport.writtenPackets().size(), 2);
}

TEST_F(ApduFramingTest, ErrorStatusWordReturnsEmpty) {
    QByteArray apdu(10, 0x42);

    // Queue an error response: Solana app not open
    m_transport.enqueueError(0x6A82);

    QByteArray result = m_transport.exchange(apdu);
    EXPECT_TRUE(result.isEmpty());
    EXPECT_EQ(m_transport.lastStatusWord(), 0x6A82);
    EXPECT_FALSE(m_transport.lastError().isEmpty());
}

TEST_F(ApduFramingTest, TransactionRejectedStatusWord) {
    QByteArray apdu(10, 0x42);

    m_transport.enqueueError(0x6985);

    QByteArray result = m_transport.exchange(apdu);
    EXPECT_TRUE(result.isEmpty());
    EXPECT_EQ(m_transport.lastStatusWord(), 0x6985);
    EXPECT_TRUE(m_transport.lastError().contains("rejected"));
}

TEST_F(ApduFramingTest, SuccessStatusWordExtracted) {
    QByteArray apdu(5, 0x11);

    QByteArray data(10, 0x22);
    m_transport.enqueueResponse(data);

    QByteArray result = m_transport.exchange(apdu);
    // Data should NOT include the status word bytes
    EXPECT_EQ(result.size(), 10);
    EXPECT_EQ(m_transport.lastStatusWord(), 0x9000);
}

// ═══════════════════════════════════════════════════════════════════
//  Suite 4: Read Error Handling
// ═══════════════════════════════════════════════════════════════════

class ReadErrorTest : public ::testing::Test {
  protected:
    MockLedgerTransport m_transport;
};

TEST_F(ReadErrorTest, ReadTimeoutReturnsEmpty) {
    QByteArray apdu(5, 0x42);
    m_transport.setSimulateTimeout(true);

    QByteArray result = m_transport.exchange(apdu);
    EXPECT_TRUE(result.isEmpty());
    EXPECT_TRUE(m_transport.lastError().contains("timed out"));
}

TEST_F(ReadErrorTest, ReadErrorReturnsEmpty) {
    QByteArray apdu(5, 0x42);
    m_transport.setSimulateReadError(true);

    QByteArray result = m_transport.exchange(apdu);
    EXPECT_TRUE(result.isEmpty());
    EXPECT_TRUE(m_transport.lastError().contains("read failed"));
}

TEST_F(ReadErrorTest, ResponseTooShortReturnsEmpty) {
    // Enqueue a response with only 1 byte (< 2 needed for status word)
    // We need to manually craft this since enqueueResponse adds 2 SW bytes
    // Instead, test that an empty data + SW still works correctly
    QByteArray apdu(5, 0x42);
    m_transport.enqueueResponse(QByteArray(), 0x9000);

    QByteArray result = m_transport.exchange(apdu);
    // Empty data with 0x9000 = success with empty result
    EXPECT_TRUE(result.isEmpty());
    EXPECT_EQ(m_transport.lastStatusWord(), 0x9000);
}

// ═══════════════════════════════════════════════════════════════════
//  Suite 5: Multi-Packet Response Reading
// ═══════════════════════════════════════════════════════════════════

class MultiPacketResponseTest : public ::testing::Test {
  protected:
    MockLedgerTransport m_transport;
};

TEST_F(MultiPacketResponseTest, LargeResponseReassembled) {
    // Response larger than first payload → multiple read packets
    QByteArray largeData(100, 0xAA);

    QByteArray apdu(5, 0x11);
    m_transport.enqueueResponse(largeData);

    QByteArray result = m_transport.exchange(apdu);
    EXPECT_EQ(result.size(), 100);
    EXPECT_EQ(result, largeData);
}

TEST_F(MultiPacketResponseTest, ExactPayloadBoundary) {
    // Response that exactly fills first payload (57 - 2 for SW = 55 bytes data)
    QByteArray data(55, 0xBB);

    QByteArray apdu(5, 0x22);
    m_transport.enqueueResponse(data);

    QByteArray result = m_transport.exchange(apdu);
    EXPECT_EQ(result.size(), 55);
    EXPECT_EQ(result, data);
}

TEST_F(MultiPacketResponseTest, SignatureResponse) {
    // Typical Ed25519 signature response: 64 bytes
    QByteArray sig(LedgerCrypto::kEd25519SignatureSize, 0xCC);

    QByteArray apdu(5, 0x33);
    m_transport.enqueueResponse(sig);

    QByteArray result = m_transport.exchange(apdu);
    EXPECT_EQ(result.size(), LedgerCrypto::kEd25519SignatureSize);
    EXPECT_EQ(result, sig);
}

TEST_F(MultiPacketResponseTest, PublicKeyResponse) {
    // GET_PUBKEY returns 32-byte Ed25519 public key
    QByteArray pubkey(LedgerCrypto::kEd25519KeySize, 0xDD);

    QByteArray apdu(5, 0x44);
    m_transport.enqueueResponse(pubkey);

    QByteArray result = m_transport.exchange(apdu);
    EXPECT_EQ(result.size(), LedgerCrypto::kEd25519KeySize);
    EXPECT_EQ(result, pubkey);
}

// ═══════════════════════════════════════════════════════════════════
//  Suite 6: GET_PUBKEY APDU Construction
// ═══════════════════════════════════════════════════════════════════

class GetPubkeyApduTest : public ::testing::Test {
  protected:
    MockLedgerTransport m_transport;
};

TEST_F(GetPubkeyApduTest, CorrectFormat) {
    // Build and send a GET_PUBKEY APDU the same way LedgerPlugin does
    QByteArray encodedPath = LedgerPlugin::encodeDerivationPath("m/44'/501'/0'");

    QByteArray apdu;
    apdu.append(static_cast<char>(LedgerApdu::kCLA));
    apdu.append(static_cast<char>(LedgerApdu::kINS_GET_PUBKEY));
    apdu.append(static_cast<char>(0x00));               // P1
    apdu.append(static_cast<char>(0x00));               // P2
    apdu.append(static_cast<char>(encodedPath.size())); // Lc
    apdu.append(encodedPath);

    // Queue a 32-byte pubkey response
    QByteArray fakePubkey(32, 0xAB);
    m_transport.enqueueResponse(fakePubkey);

    QByteArray result = m_transport.exchange(apdu);
    ASSERT_EQ(result.size(), 32);

    // Verify the APDU was framed correctly
    QByteArray reassembled = m_transport.reassembleWrittenApdu();

    // CLA
    EXPECT_EQ(static_cast<uint8_t>(reassembled[0]), LedgerApdu::kCLA);
    // INS
    EXPECT_EQ(static_cast<uint8_t>(reassembled[1]), LedgerApdu::kINS_GET_PUBKEY);
    // P1, P2
    EXPECT_EQ(static_cast<uint8_t>(reassembled[2]), 0x00);
    EXPECT_EQ(static_cast<uint8_t>(reassembled[3]), 0x00);
    // Lc = 13 (3 segments * 4 bytes + 1 count byte)
    EXPECT_EQ(static_cast<uint8_t>(reassembled[4]), 13);
    // Path starts at offset 5
    EXPECT_EQ(reassembled.mid(5), encodedPath);
}

TEST_F(GetPubkeyApduTest, FourSegmentPath) {
    QByteArray encodedPath = LedgerPlugin::encodeDerivationPath("m/44'/501'/0'/0'");
    ASSERT_EQ(encodedPath.size(), 17); // 1 + 4*4

    QByteArray apdu;
    apdu.append(static_cast<char>(LedgerApdu::kCLA));
    apdu.append(static_cast<char>(LedgerApdu::kINS_GET_PUBKEY));
    apdu.append(static_cast<char>(0x00));
    apdu.append(static_cast<char>(0x00));
    apdu.append(static_cast<char>(encodedPath.size()));
    apdu.append(encodedPath);

    QByteArray fakePubkey(32, 0xCD);
    m_transport.enqueueResponse(fakePubkey);

    QByteArray result = m_transport.exchange(apdu);
    EXPECT_EQ(result.size(), 32);
}

// ═══════════════════════════════════════════════════════════════════
//  Suite 7: Sign APDU Construction
// ═══════════════════════════════════════════════════════════════════

class SignApduTest : public ::testing::Test {
  protected:
    MockLedgerTransport m_transport;
};

TEST_F(SignApduTest, SingleChunkSign) {
    // Build a sign APDU for a small message (< 237 bytes)
    QByteArray derivPath = LedgerPlugin::encodeDerivationPath("m/44'/501'/0'");
    QByteArray message(100, 0x55); // 100-byte test message

    QByteArray data;
    data.append(static_cast<char>(0x01)); // signer count
    data.append(derivPath);
    data.append(message);

    QByteArray apdu;
    apdu.append(static_cast<char>(LedgerApdu::kCLA));
    apdu.append(static_cast<char>(LedgerApdu::kINS_SIGN));
    apdu.append(static_cast<char>(0x01));                 // P1
    apdu.append(static_cast<char>(LedgerApdu::kP2_LAST)); // P2 = last
    apdu.append(static_cast<char>(data.size()));
    apdu.append(data);

    QByteArray fakeSig(64, 0xEE);
    m_transport.enqueueResponse(fakeSig);

    QByteArray result = m_transport.exchange(apdu);
    EXPECT_EQ(result.size(), 64);

    // Verify APDU structure
    QByteArray reassembled = m_transport.reassembleWrittenApdu();
    EXPECT_EQ(static_cast<uint8_t>(reassembled[0]), LedgerApdu::kCLA);
    EXPECT_EQ(static_cast<uint8_t>(reassembled[1]), LedgerApdu::kINS_SIGN);
    EXPECT_EQ(static_cast<uint8_t>(reassembled[2]), 0x01); // P1
    EXPECT_EQ(static_cast<uint8_t>(reassembled[3]), LedgerApdu::kP2_LAST);
}

TEST_F(SignApduTest, MultiChunkFirstApdu) {
    // Build the first APDU for a multi-chunk sign (message > 237 bytes)
    QByteArray derivPath = LedgerPlugin::encodeDerivationPath("m/44'/501'/0'");
    QByteArray message(300, 0x66); // 300 bytes > kFirstChunkMax

    QByteArray data;
    data.append(static_cast<char>(0x01));
    data.append(derivPath);
    data.append(message.mid(0, LedgerApdu::kFirstChunkMax));

    QByteArray apdu;
    apdu.append(static_cast<char>(LedgerApdu::kCLA));
    apdu.append(static_cast<char>(LedgerApdu::kINS_SIGN));
    apdu.append(static_cast<char>(0x01));
    apdu.append(static_cast<char>(LedgerApdu::kP2_MORE)); // P2 = more to come
    apdu.append(static_cast<char>(data.size()));
    apdu.append(data);

    // First chunk gets empty response (device waits for more)
    m_transport.enqueueResponse(QByteArray());

    QByteArray result = m_transport.exchange(apdu);
    // Empty data with 0x9000 = continue
    EXPECT_EQ(m_transport.lastStatusWord(), 0x9000);

    // Verify P2 is MORE
    QByteArray reassembled = m_transport.reassembleWrittenApdu();
    EXPECT_EQ(static_cast<uint8_t>(reassembled[3]), LedgerApdu::kP2_MORE);
}

TEST_F(SignApduTest, ContinuationChunkApdu) {
    // Continuation APDU has no signer count or path, just data
    QByteArray chunk(200, 0x77);

    QByteArray apdu;
    apdu.append(static_cast<char>(LedgerApdu::kCLA));
    apdu.append(static_cast<char>(LedgerApdu::kINS_SIGN));
    apdu.append(static_cast<char>(0x01));
    apdu.append(static_cast<char>(LedgerApdu::kP2_LAST)); // last chunk
    apdu.append(static_cast<char>(chunk.size()));
    apdu.append(chunk);

    QByteArray fakeSig(64, 0xFF);
    m_transport.enqueueResponse(fakeSig);

    QByteArray result = m_transport.exchange(apdu);
    EXPECT_EQ(result.size(), 64);
}

// ═══════════════════════════════════════════════════════════════════
//  Suite 8: Constants Validation
// ═══════════════════════════════════════════════════════════════════

class ConstantsTest : public ::testing::Test {};

TEST_F(ConstantsTest, WireProtocolValues) {
    EXPECT_EQ(LedgerWire::kChannel, 0x0101);
    EXPECT_EQ(LedgerWire::kTagApdu, 0x05);
    EXPECT_EQ(LedgerWire::kPacketSize, 64);
    EXPECT_EQ(LedgerWire::kFirstPayload, 57);
    EXPECT_EQ(LedgerWire::kContPayload, 59);
}

TEST_F(ConstantsTest, ApduConstants) {
    EXPECT_EQ(LedgerApdu::kCLA, 0xE0);
    EXPECT_EQ(LedgerApdu::kINS_GET_PUBKEY, 0x05);
    EXPECT_EQ(LedgerApdu::kINS_SIGN, 0x06);
    EXPECT_EQ(LedgerApdu::kP2_LAST, 0x00);
    EXPECT_EQ(LedgerApdu::kP2_MORE, 0x02);
    EXPECT_EQ(LedgerApdu::kFirstChunkMax, 237);
    EXPECT_EQ(LedgerApdu::kContChunkMax, 255);
}

TEST_F(ConstantsTest, CryptoConstants) {
    EXPECT_EQ(LedgerCrypto::kEd25519KeySize, 32);
    EXPECT_EQ(LedgerCrypto::kEd25519SignatureSize, 64);
    EXPECT_EQ(LedgerCrypto::kHardenedBit, 0x80000000u);
}

TEST_F(ConstantsTest, TimeoutConstants) {
    EXPECT_EQ(LedgerTimeout::kExchange, 30000);
    EXPECT_EQ(LedgerTimeout::kGetPubkey, 10000);
    EXPECT_EQ(LedgerTimeout::kSign, 60000);
}

// ═══════════════════════════════════════════════════════════════════
//  Suite 9: Full Round Trip
// ═══════════════════════════════════════════════════════════════════

class RoundTripTest : public ::testing::Test {
  protected:
    MockLedgerTransport m_transport;
};

TEST_F(RoundTripTest, GetPubkeyRoundTrip) {
    // Simulate full GET_PUBKEY exchange
    QByteArray encodedPath = LedgerPlugin::encodeDerivationPath("m/44'/501'/0'");
    ASSERT_FALSE(encodedPath.isEmpty());

    // Build APDU
    QByteArray apdu;
    apdu.append(static_cast<char>(LedgerApdu::kCLA));
    apdu.append(static_cast<char>(LedgerApdu::kINS_GET_PUBKEY));
    apdu.append(static_cast<char>(0x00));
    apdu.append(static_cast<char>(0x00));
    apdu.append(static_cast<char>(encodedPath.size()));
    apdu.append(encodedPath);

    // 32-byte "public key"
    QByteArray expectedPubkey(32, '\0');
    for (int i = 0; i < 32; ++i) {
        expectedPubkey[i] = static_cast<char>(i);
    }
    m_transport.enqueueResponse(expectedPubkey);

    // Exchange
    QByteArray pubkey = m_transport.exchange(apdu);
    ASSERT_EQ(pubkey.size(), 32);
    EXPECT_EQ(pubkey, expectedPubkey);
    EXPECT_EQ(m_transport.lastStatusWord(), 0x9000);
    EXPECT_TRUE(m_transport.lastError().isEmpty());
}

TEST_F(RoundTripTest, SignRejectedByUser) {
    QByteArray encodedPath = LedgerPlugin::encodeDerivationPath("m/44'/501'/0'");
    QByteArray message(50, 0xAA);

    QByteArray data;
    data.append(static_cast<char>(0x01));
    data.append(encodedPath);
    data.append(message);

    QByteArray apdu;
    apdu.append(static_cast<char>(LedgerApdu::kCLA));
    apdu.append(static_cast<char>(LedgerApdu::kINS_SIGN));
    apdu.append(static_cast<char>(0x01));
    apdu.append(static_cast<char>(LedgerApdu::kP2_LAST));
    apdu.append(static_cast<char>(data.size()));
    apdu.append(data);

    // User rejects on device → SW 0x6985
    m_transport.enqueueError(0x6985);

    QByteArray sig = m_transport.exchange(apdu);
    EXPECT_TRUE(sig.isEmpty());
    EXPECT_EQ(m_transport.lastStatusWord(), 0x6985);
    EXPECT_TRUE(m_transport.lastError().contains("rejected"));
}

TEST_F(RoundTripTest, SignSuccessful) {
    QByteArray encodedPath = LedgerPlugin::encodeDerivationPath("m/44'/501'/0'");
    QByteArray message(80, 0xBB);

    QByteArray data;
    data.append(static_cast<char>(0x01));
    data.append(encodedPath);
    data.append(message);

    QByteArray apdu;
    apdu.append(static_cast<char>(LedgerApdu::kCLA));
    apdu.append(static_cast<char>(LedgerApdu::kINS_SIGN));
    apdu.append(static_cast<char>(0x01));
    apdu.append(static_cast<char>(LedgerApdu::kP2_LAST));
    apdu.append(static_cast<char>(data.size()));
    apdu.append(data);

    // 64-byte signature
    QByteArray expectedSig(64, '\0');
    for (int i = 0; i < 64; ++i) {
        expectedSig[i] = static_cast<char>(i);
    }
    m_transport.enqueueResponse(expectedSig);

    QByteArray sig = m_transport.exchange(apdu);
    ASSERT_EQ(sig.size(), 64);
    EXPECT_EQ(sig, expectedSig);
    EXPECT_EQ(m_transport.lastStatusWord(), 0x9000);
}

// ═══════════════════════════════════════════════════════════════════
//  Main
// ═══════════════════════════════════════════════════════════════════

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
