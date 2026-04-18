#ifndef TREZORHIDTRANSPORT_H
#define TREZORHIDTRANSPORT_H

#include "crypto/plugin/trezor/TrezorTransport.h"

struct hid_device_;
typedef struct hid_device_ hid_device;

// HID transport for Trezor One (VID 0x534C, PID 0x0001)
class TrezorHidTransport : public TrezorTransport {
  public:
    TrezorHidTransport();
    ~TrezorHidTransport() override;

    bool open(const QString& devicePath) override;
    void close() override;
    bool isOpen() const override;

  protected:
    bool writePacket(const uint8_t* data, int size) override;
    int readPacket(uint8_t* buf, int bufSize, int timeoutMs) override;
    int writePacketSize() const override;
    int readPacketSize() const override;
    int writeDataOffset() const override;

  private:
    hid_device* m_device = nullptr;
};

#endif // TREZORHIDTRANSPORT_H
