#pragma once

#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace d2xx {

// ---------------------------------------------------------------------------
// MpsseError  —  thrown on any D2XX / MPSSE failure
// ---------------------------------------------------------------------------
class MpsseError : public std::runtime_error {
public:
    explicit MpsseError(const std::string& msg, unsigned ftStatus = 0)
        : std::runtime_error(msg), status_(ftStatus) {}

    /// FT_STATUS value returned by the failing D2XX call (0 = FT_OK / unknown).
    unsigned status() const noexcept { return status_; }

private:
    unsigned status_;
};

// ---------------------------------------------------------------------------
// MpsseDevice  —  RAII wrapper for a D2XX device in MPSSE mode
//
//  Responsibilities:
//   - Open the device by index (FT_Open)
//   - Configure USB transfer parameters, timeouts, and latency timer
//   - Enable MPSSE mode (FT_SetBitMode)
//   - Synchronise the MPSSE engine (0xAA / 0xAB echo protocol)
//   - Configure the SCK clock frequency (60 MHz base, divisor method)
//   - Provide raw write / read primitives for MPSSE command bytes
//   - Close the device on destruction (RAII)
//
//  Pin assignment (FT4232H CH-B low byte — same for CH-A/C/D at their DBUSx):
//    bit 0  BDBUS0  SCK   (clock out)
//    bit 1  BDBUS1  MOSI  (data out, FTDI "DO")
//    bit 2  BDBUS2  MISO  (data in,  FTDI "DI")
//    bit 3  BDBUS3  CS#   (chip-select, active-low)
//    bit 4..7 BDBUS4-7   GPIO (managed by Gpio class)
//
//  High byte (BCBUS0-7) is pure GPIO and managed entirely by Gpio.
// ---------------------------------------------------------------------------
class MpsseDevice {
public:
    /// @param channelIndex  Index into the D2XX device list (0 = first device).
    ///                      FT4232H with a single chip: CH_A=0, CH_B=1, CH_C=2, CH_D=3.
    /// @param clockHz       Desired SCK frequency in Hz (default 1 MHz).
    explicit MpsseDevice(int channelIndex = 1, uint32_t clockHz = 1'000'000);
    ~MpsseDevice() noexcept;

    MpsseDevice(const MpsseDevice&) = delete;
    MpsseDevice& operator=(const MpsseDevice&) = delete;
    MpsseDevice(MpsseDevice&&) noexcept;
    MpsseDevice& operator=(MpsseDevice&&) noexcept;

    // -----------------------------------------------------------------------
    // Low-level I/O  (MPSSE command bytes)
    // -----------------------------------------------------------------------

    /// Write raw MPSSE command bytes to the device.
    void write(std::span<const uint8_t> data);

    /// Read exactly `count` bytes from the device RX queue.
    /// Polls FT_GetQueueStatus; throws MpsseError on timeout.
    std::vector<uint8_t> read(size_t count, uint32_t timeoutMs = 3000);

    /// Send the MPSSE "Send Immediate" command (0x87) to flush the USB packet.
    /// Call after queuing commands that expect a response.
    void flush();

    /// Read GPIO pin states through the MPSSE engine.
    /// @param highByte false=low byte (0x81), true=high byte (0x83)
    uint8_t readPins(bool highByte = false);

    // -----------------------------------------------------------------------
    // Direct D2XX handle access
    // -----------------------------------------------------------------------

    /// Returns the raw FT_HANDLE (typed as void*) for direct D2XX API calls
    /// such as FT_ReadEE.  Do not close the handle; MpsseDevice owns it.
    void* handle() const noexcept { return handle_; }

    /// Read the instantaneous pin state of the low byte via FT_GetBitMode.
    /// In MPSSE mode this returns the last-written output register value for
    /// output pins, allowing software verification of GPIO commands.
    /// For GPIO readback via MPSSE commands, prefer readPins().
    /// bit0=SCK, bit1=MOSI, bit2=MISO, bit3=CS#, bit4-7=GPIO
    uint8_t getBitMode();

    // -----------------------------------------------------------------------
    // Utility
    // -----------------------------------------------------------------------

    /// Print information about all connected D2XX devices to stdout.
    static void listDevices();

private:
    void* handle_{nullptr};

    void initMpsse(uint32_t clockHz);
    void syncMpsse();
    void checkStatus(unsigned st, const char* ctx);
};

} // namespace d2xx
