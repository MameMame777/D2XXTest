#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "MpsseDevice.hpp"

namespace d2xx {

// ---------------------------------------------------------------------------
// SpiMaster  —  SPI master (Mode 0, MSB first) over MPSSE
//
//  MPSSE command used for transfer:
//    0x31  Clock Data Bytes In+Out, MSB first, out on falling edge, in on rising.
//    This matches SPI Mode 0: CPOL=0 (SCK idles LOW), CPHA=0 (capture on rising).
//
//  CS# is controlled manually with csLow() / csHigh() (GPIO via 0x80 command).
//  The caller is responsible for asserting/deasserting CS around each transfer.
//
//  GPIO bits 4-7 of the low byte are left untouched by SpiMaster; use the Gpio
//  class to manage them independently.
// ---------------------------------------------------------------------------
class SpiMaster {
public:
    /// Construct a SpiMaster on the given MpsseDevice.
    /// Sets the low-byte GPIO to SPI idle state: CS#=High, SCK=Low, MOSI=Low.
    explicit SpiMaster(MpsseDevice& dev);

    // -----------------------------------------------------------------------
    // SPI transfer
    // -----------------------------------------------------------------------

    /// Full-duplex transfer of `len` bytes.
    /// @param tx   Transmit buffer (MOSI).  Must point to at least `len` bytes.
    /// @param rx   Receive buffer  (MISO).  Must point to at least `len` bytes.
    ///             May be nullptr if received data should be discarded.
    /// @param len  Number of bytes to transfer (max 65536 per call).
    void transfer(const uint8_t* tx, uint8_t* rx, size_t len);

    // -----------------------------------------------------------------------
    // Chip-select control (call before / after transfer)
    // -----------------------------------------------------------------------
    void csLow();   ///< Assert CS# (drive BDBUS3 LOW).
    void csHigh();  ///< Deassert CS# (drive BDBUS3 HIGH).

    // -----------------------------------------------------------------------
    // Loopback (MPSSE internal DO→DI connection; no external wiring needed)
    // -----------------------------------------------------------------------
    /// Enable (true) or disable (false) the MPSSE internal loopback.
    /// While enabled, data clocked out on MOSI is immediately looped back to MISO.
    void enableLoopback(bool enable);

    // -----------------------------------------------------------------------
    // Low-byte GPIO access (bits 4-7 only; bits 0-3 reserved for SPI)
    // -----------------------------------------------------------------------
    /// Write bits 4-7 of the low byte without disturbing the SPI pins.
    /// @param value     New bit values for bits 4-7 (bit 0 of nibble = BDBUS4).
    /// @param dirMask   Direction mask for bits 4-7  (1=output, 0=input).
    void setUpperNibble(uint8_t value, uint8_t dirMask);

private:
    MpsseDevice& dev_;
    uint8_t lowValue_{0x08};      ///< Current low-byte output value (CS=1, rest=0)
    uint8_t lowDirection_{0x0B};  ///< Low-byte direction: bits 0,1,3=out; bit2=in

    void applyLowByte();
};

} // namespace d2xx
