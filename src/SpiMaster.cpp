#include "d2xx_mpsse/SpiMaster.hpp"

#include <cstdint>
#include <vector>

namespace d2xx {

SpiMaster::SpiMaster(MpsseDevice& dev)
    : dev_(dev) {
    // Establish known SPI idle state on the low byte:
    //   SCK=0 (bit0), MOSI=0 (bit1), MISO=in (bit2), CS#=1 (bit3)
    applyLowByte();
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void SpiMaster::applyLowByte() {
    const uint8_t cmd[3] = { 0x80u, lowValue_, lowDirection_ };
    dev_.write(std::span<const uint8_t>(cmd));
}

// ---------------------------------------------------------------------------
// CS# control
// ---------------------------------------------------------------------------

void SpiMaster::csLow() {
    lowValue_ &= static_cast<uint8_t>(~0x08u); // Clear bit 3
    applyLowByte();
}

void SpiMaster::csHigh() {
    lowValue_ |= 0x08u; // Set bit 3
    applyLowByte();
}

// ---------------------------------------------------------------------------
// Loopback
// ---------------------------------------------------------------------------

void SpiMaster::enableLoopback(bool enable) {
    const uint8_t cmd = enable ? 0x84u : 0x85u;
    dev_.write(std::span<const uint8_t>(&cmd, 1));
}

// ---------------------------------------------------------------------------
// SPI transfer (Mode 0, MSB first)
// ---------------------------------------------------------------------------

void SpiMaster::transfer(const uint8_t* tx, uint8_t* rx, size_t len) {
    if (len == 0) return;

    // MPSSE command 0x31:
    //   Clock Data Bytes In + Out, MSB first
    //   OUT: clocked on falling edge of SCK
    //   IN:  latched  on rising  edge of SCK
    //   → SPI Mode 0 (CPOL=0, CPHA=0) ✓
    //
    // Byte layout:  [0x31] [lenLow] [lenHigh] [data0 … dataN-1] [0x87]
    //   lenLow / lenHigh = (N-1) encoded as little-endian 16-bit integer.
    //   0x87 = "Send Immediate" — forces the chip to flush the response
    //          without waiting for the USB latency timer.

    const uint32_t n = static_cast<uint32_t>(len);

    std::vector<uint8_t> cmd;
    cmd.reserve(3 + len + 1);

    cmd.push_back(0x31u);
    cmd.push_back(static_cast<uint8_t>((n - 1u) & 0xFFu));
    cmd.push_back(static_cast<uint8_t>(((n - 1u) >> 8u) & 0xFFu));
    cmd.insert(cmd.end(), tx, tx + len);
    cmd.push_back(0x87u); // Send Immediate

    dev_.write(std::span<const uint8_t>(cmd.data(), cmd.size()));

    // Read back the N bytes clocked in from MISO
    std::vector<uint8_t> resp = dev_.read(len);

    if (rx) {
        std::copy(resp.begin(), resp.end(), rx);
    }
}

// ---------------------------------------------------------------------------
// Upper nibble GPIO (bits 4-7 of the low byte)
// ---------------------------------------------------------------------------

void SpiMaster::setUpperNibble(uint8_t value, uint8_t dirMask) {
    // Mask to bits 4-7 only, merge with current SPI pin state
    const uint8_t newVal = static_cast<uint8_t>(
        (lowValue_     & 0x0Fu) | static_cast<uint8_t>((value   & 0x0Fu) << 4));
    const uint8_t newDir = static_cast<uint8_t>(
        (lowDirection_ & 0x0Fu) | static_cast<uint8_t>((dirMask & 0x0Fu) << 4));

    lowValue_     = newVal;
    lowDirection_ = newDir;
    applyLowByte();
}

} // namespace d2xx
