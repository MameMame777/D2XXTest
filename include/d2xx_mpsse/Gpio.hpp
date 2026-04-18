#pragma once

#include <cstdint>

#include "MpsseDevice.hpp"

namespace d2xx {

// ---------------------------------------------------------------------------
// Gpio  —  MPSSE GPIO control for the low byte (DBUSx) or high byte (CBUSx)
//
//  MPSSE commands used:
//    0x80  Set Data Bits Low  Byte  (value, direction)
//    0x81  Get Data Bits Low  Byte  → 1 byte response
//    0x82  Set Data Bits High Byte  (value, direction)
//    0x83  Get Data Bits High Byte  → 1 byte response
//
//  NOTE: When SpiMaster is active, low-byte bits 0-3 (SCK, MOSI, MISO, CS#)
//        are owned by SpiMaster.  Only operate bits 4-7 of the low byte via
//        Gpio while SPI is in use, OR use SpiMaster::setUpperNibble() instead.
// ---------------------------------------------------------------------------
class Gpio {
public:
    enum class Bank {
        Low,   ///< Low byte  (ADBUS0-7 / BDBUS0-7 / ...)
        High,  ///< High byte (ACBUS0-7 / BCBUS0-7 / ...)
    };

    /// Construct a Gpio controller for the specified bank.
    /// Does not send any commands until setDirection() or write() is called.
    Gpio(MpsseDevice& dev, Bank bank);

    // -----------------------------------------------------------------------
    // Direction
    // -----------------------------------------------------------------------

    /// Set the direction mask.  1 = output, 0 = input, per bit.
    void setDirection(uint8_t dirMask);

    uint8_t direction() const noexcept { return direction_; }

    // -----------------------------------------------------------------------
    // Output
    // -----------------------------------------------------------------------

    /// Write the output register (only bits configured as outputs take effect).
    void write(uint8_t value);

    /// Set a single output pin (0-7).
    void setPin(int pin, bool high);

    uint8_t outputValue() const noexcept { return value_; }

    // -----------------------------------------------------------------------
    // Input
    // -----------------------------------------------------------------------

    /// Read the current pin state (issues a read command; returns all 8 bits).
    uint8_t read();

    /// Read a single pin state (0-7).
    bool getPin(int pin);

private:
    MpsseDevice& dev_;
    Bank         bank_;
    uint8_t      direction_{0x00};  ///< Last-written direction mask
    uint8_t      value_{0x00};      ///< Last-written output value

    uint8_t cmdSet()  const noexcept { return (bank_ == Bank::Low) ? 0x80u : 0x82u; }
    uint8_t cmdRead() const noexcept { return (bank_ == Bank::Low) ? 0x81u : 0x83u; }

    void applyOutput();
};

} // namespace d2xx
