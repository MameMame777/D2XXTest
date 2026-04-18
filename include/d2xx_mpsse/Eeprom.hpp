#pragma once

#include <cstdint>
#include <vector>

#include "MpsseDevice.hpp"

namespace d2xx {

class SpiMaster; // forward-declared; full type only needed in Eeprom.cpp

// ---------------------------------------------------------------------------
// Eeprom  —  EEPROM read utilities
//
//  Two independent read paths:
//
//  1. FTDI internal EEPROM  (D2XX FT_ReadEE API, raw 16-bit word access)
//     - Works on any open FT_HANDLE regardless of MPSSE mode
//     - Constructor: Eeprom(dev)  or  Eeprom(dev, spi)
//
//  2. External SPI EEPROM  (Microchip 25LC/25AA, Atmel AT25, etc.)
//     - Uses SpiMaster for the SPI READ command (opcode 0x03)
//     - Address bytes are configurable: 1, 2, or 3
//     - Constructor: Eeprom(dev, spi)  mandatory
// ---------------------------------------------------------------------------
class Eeprom {
public:
    /// Constructor for FTDI internal EEPROM only.
    explicit Eeprom(MpsseDevice& dev);

    /// Constructor for both FTDI internal and external SPI EEPROM.
    Eeprom(MpsseDevice& dev, SpiMaster& spi);

    // -----------------------------------------------------------------------
    // FTDI internal EEPROM  (FT_ReadEE)
    // -----------------------------------------------------------------------

    /// Read `wordCount` 16-bit words starting at EEPROM word address `startWord`.
    ///
    /// Known layout (FT4232H / FT2232H, word index):
    ///   0  Device type / config byte
    ///   1  VID (default 0x0403)
    ///   2  PID (FT4232H: default 0x6011)
    ///
    /// @throws MpsseError on FT_ReadEE failure.
    std::vector<uint16_t> readFtdiInternal(uint32_t startWord, uint32_t wordCount);

    // -----------------------------------------------------------------------
    // External SPI EEPROM  (25-series READ command, no write support)
    // -----------------------------------------------------------------------

    /// Read `length` bytes starting at byte address `address`.
    ///
    /// Protocol:  CS_Low → [0x03] [addrMSB..addrLSB] [dummy×length] → CS_High
    /// Received bytes during the dummy phase are the EEPROM data.
    ///
    /// @param address    Start byte address in the EEPROM.
    /// @param length     Number of bytes to read.
    /// @param addrBytes  Number of address bytes:
    ///                   1 → up to 25xx040  (max addr 0x1FF, high bit in cmd for >256)
    ///                   2 → 25LC256/512, most common 16-bit address parts
    ///                   3 → large EEPROMs with 24-bit address (≥128 Kbit)
    ///
    /// @throws std::invalid_argument if addrBytes is not 1, 2, or 3.
    /// @throws MpsseError on SPI / D2XX failure.
    /// @throws std::logic_error if constructed without a SpiMaster.
    std::vector<uint8_t> readSpiEeprom(uint32_t address, size_t length,
                                       int addrBytes = 2);

private:
    MpsseDevice& dev_;
    SpiMaster*   spi_{nullptr}; ///< Optional; required for readSpiEeprom()
};

} // namespace d2xx
