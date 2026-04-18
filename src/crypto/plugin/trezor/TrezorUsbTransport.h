#ifndef TREZORUSBTRANSPORT_H
#define TREZORUSBTRANSPORT_H

#include "crypto/plugin/trezor/TrezorTransport.h"

#include <QList>
#include <QMutex>

struct libusb_context;
struct libusb_device_handle;

// WebUSB (libusb) transport for Trezor Model T / Safe 3 / Safe 5
// These devices use vendor-specific USB interface (class 0xFF) with
// interrupt endpoints, NOT HID. Uses interface 0, endpoints 0x01 (OUT) / 0x81 (IN).

class TrezorUsbTransport : public TrezorTransport {
  public:
    TrezorUsbTransport();
    ~TrezorUsbTransport() override;

    bool open(const QString& devicePath) override;
    void close() override;
    bool isOpen() const override;

    // Static helper for device enumeration
    struct UsbDeviceInfo {
        QString path; // "bus:addr" format
        uint16_t vid = 0;
        uint16_t pid = 0;
        QString product;
        QString manufacturer;
    };
    static QList<UsbDeviceInfo> enumerate(uint16_t vid, uint16_t pid);

  protected:
    bool writePacket(const uint8_t* data, int size) override;
    int readPacket(uint8_t* buf, int bufSize, int timeoutMs) override;
    int writePacketSize() const override;
    int readPacketSize() const override;
    int writeDataOffset() const override;

  private:
    static constexpr uint8_t kEndpointOut = 0x01;
    static constexpr uint8_t kEndpointIn = 0x81;
    static constexpr int kInterfaceNum = 0;

    // Shared libusb context — ref-counted so the kernel doesn't reclaim the
    // interface between consecutive open/close cycles on macOS.
    static libusb_context* s_ctx;
    static int s_ctxRefCount;
    static QMutex s_ctxMutex;
    static libusb_context* acquireContext();
    static void releaseContext();

    libusb_device_handle* m_handle = nullptr;
    bool m_interfaceClaimed = false;
};

#endif // TREZORUSBTRANSPORT_H
