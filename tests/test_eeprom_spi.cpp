// test_eeprom_spi.cpp
//
// Reads bytes from an external SPI EEPROM (25-series: Microchip 25LC/25AA,
// Atmel AT25, etc.) via MPSSE SPI.
//
// SPI READ protocol (opcode 0x03):
//   CS_Low → [0x03] [AddrMSB..AddrLSB] [0x00 × length] → CS_High
//   EEPROM drives MISO with data during the dummy byte phase.
//
// Wiring (FT4232H CH-B):
//   BDBUS0 → EEPROM SCK
//   BDBUS1 → EEPROM SI (MOSI)
//   BDBUS2 ← EEPROM SO (MISO)
//   BDBUS3 → EEPROM CS# (active-low)
//   3.3V / GND to VCC, VSS, WP#=VCC, HOLD#=VCC
//
// Usage:
//   test_eeprom_spi [--channel N] [--addr ADDR] [--len N] [--addrbytes N]
//
//   --channel   N   D2XX device index (default 1 = FT4232H CH-B)
//   --addr      N   Start byte address, hex or decimal (default 0x000000)
//   --len       N   Number of bytes to read (default 32)
//   --addrbytes N   Address byte count: 1, 2, or 3 (default 2 for 25LC256)
//                     1 → up to 25xx040  (16-bit parts with 9-bit addr)
//                     2 → 25LC256/512    (16-bit address, most common)
//                     3 → large EEPROMs  (24-bit address, ≥128 Kbit)

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#include <vector>

#include "d2xx_mpsse/Eeprom.hpp"
#include "d2xx_mpsse/MpsseDevice.hpp"
#include "d2xx_mpsse/SpiMaster.hpp"

// ---------------------------------------------------------------------------
static void hexDump(const std::vector<uint8_t>& data, uint32_t baseAddr) {
    constexpr int cols = 16;
    for (std::size_t i = 0; i < data.size(); ++i) {
        if (i % static_cast<std::size_t>(cols) == 0)
            std::printf("  0x%06X  ", static_cast<unsigned>(baseAddr + i));
        std::printf("%02X ", data[i]);
        if (i % static_cast<std::size_t>(cols) ==
                static_cast<std::size_t>(cols - 1) ||
            i == data.size() - 1) {
            // ASCII side panel
            const std::size_t rowStart =
                (i / static_cast<std::size_t>(cols)) *
                static_cast<std::size_t>(cols);
            const std::size_t rowEnd = i + 1;
            const int pad =
                cols - static_cast<int>(rowEnd - rowStart);
            for (int p = 0; p < pad; ++p) std::printf("   ");
            std::printf(" |");
            for (std::size_t j = rowStart; j < rowEnd; ++j) {
                const uint8_t c = data[j];
                std::printf("%c",
                    (c >= 0x20u && c < 0x7Fu) ? static_cast<char>(c) : '.');
            }
            std::printf("|\n");
        }
    }
}

// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    int      channelIndex = 1;
    uint32_t address      = 0x000000u;
    std::size_t length    = 32;
    int      addrBytes    = 2;

    for (int i = 1; i < argc; ++i) {
        const std::string a(argv[i]);
        if (a == "--channel"   && i + 1 < argc)
            channelIndex = std::atoi(argv[++i]);
        else if (a == "--addr" && i + 1 < argc)
            address = static_cast<uint32_t>(
                std::stoul(argv[++i], nullptr, 0));
        else if (a == "--len" && i + 1 < argc)
            length = static_cast<std::size_t>(
                std::stoul(argv[++i], nullptr, 0));
        else if (a == "--addrbytes" && i + 1 < argc)
            addrBytes = std::atoi(argv[++i]);
    }

    std::printf("=== External SPI EEPROM Read ===\n");
    std::printf("  Channel index : %d\n",    channelIndex);
    std::printf("  Start address : 0x%06X\n", address);
    std::printf("  Length        : %zu bytes\n", length);
    std::printf("  Address bytes : %d\n",     addrBytes);
    std::printf("\n");

    try {
        d2xx::MpsseDevice dev(channelIndex, 1'000'000);
        d2xx::SpiMaster   spi(dev);
        d2xx::Eeprom      eeprom(dev, spi);

        const auto data = eeprom.readSpiEeprom(address, length, addrBytes);
        hexDump(data, address);

        // Diagnostics
        const bool allFF =
            std::all_of(data.begin(), data.end(),
                        [](uint8_t b) { return b == 0xFFu; });
        const bool allZero =
            std::all_of(data.begin(), data.end(),
                        [](uint8_t b) { return b == 0x00u; });

        std::printf("\n");
        if (allFF) {
            std::printf("  NOTE: All bytes are 0xFF — "
                        "EEPROM may not be connected (MISO floating high),\n"
                        "        or the address range is erased/empty.\n");
        } else if (allZero) {
            std::printf("  NOTE: All bytes are 0x00 — "
                        "MISO may be pulled low, or EEPROM is blank-programmed.\n");
        } else {
            std::printf("  Data read OK.\n");
        }

    } catch (const d2xx::MpsseError& e) {
        std::fprintf(stderr, "[ERROR] %s  (FT_STATUS=%u)\n",
                     e.what(), e.status());
        return 1;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[ERROR] %s\n", e.what());
        return 1;
    }

    return 0;
}
