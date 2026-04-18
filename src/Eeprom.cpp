#include "d2xx_mpsse/Eeprom.hpp"
#include "d2xx_mpsse/SpiMaster.hpp"

#include <ftd2xx.h>

#include <cstdio>
#include <stdexcept>
#include <vector>

namespace d2xx {

// ---------------------------------------------------------------------------
// Constructors
// ---------------------------------------------------------------------------

Eeprom::Eeprom(MpsseDevice& dev)
    : dev_(dev), spi_(nullptr) {}

Eeprom::Eeprom(MpsseDevice& dev, SpiMaster& spi)
    : dev_(dev), spi_(&spi) {}

// ---------------------------------------------------------------------------
// FTDI internal EEPROM  —  FT_ReadEE (one word at a time)
// ---------------------------------------------------------------------------

std::vector<uint16_t> Eeprom::readFtdiInternal(uint32_t startWord,
                                                uint32_t wordCount) {
    auto* h = static_cast<FT_HANDLE>(dev_.handle());

    std::vector<uint16_t> result;
    result.reserve(wordCount);

    for (uint32_t i = 0; i < wordCount; ++i) {
        WORD val = 0;
        const FT_STATUS st = FT_ReadEE(h, static_cast<DWORD>(startWord + i), &val);
        if (st != FT_OK) {
            throw MpsseError(
                "FT_ReadEE failed at word address 0x" +
                [&] {
                    char buf[12];
                    std::snprintf(buf, sizeof(buf), "%04X", startWord + i);
                    return std::string(buf);
                }(),
                static_cast<unsigned>(st));
        }
        result.push_back(static_cast<uint16_t>(val));
    }

    return result;
}

// ---------------------------------------------------------------------------
// External SPI EEPROM  —  25-series READ command (0x03)
// ---------------------------------------------------------------------------

std::vector<uint8_t> Eeprom::readSpiEeprom(uint32_t address, size_t length,
                                            int addrBytes) {
    if (!spi_) {
        throw std::logic_error(
            "Eeprom::readSpiEeprom: SpiMaster not provided at construction");
    }
    if (addrBytes < 1 || addrBytes > 3) {
        throw std::invalid_argument(
            "Eeprom::readSpiEeprom: addrBytes must be 1, 2, or 3");
    }
    if (length == 0) return {};

    // Total SPI transaction:
    //   [0x03]  READ opcode
    //   [A2][A1][A0]  address bytes, MSB first (addrBytes bytes)
    //   [D0..Dn-1]    dummy TX bytes (0x00); EEPROM drives MISO with data
    const std::size_t cmdLen   = 1u + static_cast<std::size_t>(addrBytes);
    const std::size_t totalLen = cmdLen + length;

    std::vector<uint8_t> txBuf(totalLen, 0x00u);
    std::vector<uint8_t> rxBuf(totalLen, 0x00u);

    txBuf[0] = 0x03u; // READ command

    // Fill address bytes MSB-first
    for (int i = addrBytes - 1; i >= 0; --i) {
        const std::size_t bytePos =
            1u + static_cast<std::size_t>(addrBytes - 1 - i);
        txBuf[bytePos] = static_cast<uint8_t>((address >> (8 * i)) & 0xFFu);
    }
    // Remaining bytes are already 0x00 (dummy for data phase)

    spi_->csLow();
    spi_->transfer(txBuf.data(), rxBuf.data(), totalLen);
    spi_->csHigh();

    // Return only the data portion (skip cmd + address echo bytes in rx)
    return std::vector<uint8_t>(
        rxBuf.begin() + static_cast<std::ptrdiff_t>(cmdLen),
        rxBuf.end());
}

} // namespace d2xx
