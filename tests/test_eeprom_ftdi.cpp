// test_eeprom_ftdi.cpp
//
// Reads and dumps the FTDI chip's internal EEPROM using the D2XX FT_ReadEE API.
// No SPI device or special wiring needed.
//
// Standard layout of FT4232H / FT2232H internal EEPROM (first few words):
//   Word 0x00  Device type / config
//   Word 0x01  Vendor ID  (default 0x0403)
//   Word 0x02  Product ID (FT4232H: default 0x6011, FT2232H: 0x6010)
//
// Usage:
//   test_eeprom_ftdi [--channel N] [--start ADDR] [--count N]
//
//   --channel N   D2XX device index (default 1 = FT4232H CH-B)
//   --start  N    First word address in hex or decimal (default 0)
//   --count  N    Number of 16-bit words to read (default 64 = 0x00-0x3F)

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#include <vector>

#include "d2xx_mpsse/Eeprom.hpp"
#include "d2xx_mpsse/MpsseDevice.hpp"
#include "d2xx_mpsse/SpiMaster.hpp"

// ---------------------------------------------------------------------------
static void hexDump(const std::vector<uint16_t>& words, uint32_t baseWord) {
    constexpr int cols = 8;
    for (std::size_t i = 0; i < words.size(); ++i) {
        if (i % static_cast<std::size_t>(cols) == 0)
            std::printf("  [0x%03X]  ", static_cast<unsigned>(baseWord + i));
        std::printf("0x%04X ", words[i]);
        if (i % static_cast<std::size_t>(cols) == static_cast<std::size_t>(cols - 1) ||
            i == words.size() - 1)
            std::printf("\n");
    }
}

// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    int      channelIndex = 1;
    uint32_t startWord    = 0;
    uint32_t wordCount    = 64;

    for (int i = 1; i < argc; ++i) {
        const std::string a(argv[i]);
        if (a == "--channel" && i + 1 < argc)
            channelIndex = std::atoi(argv[++i]);
        else if (a == "--start" && i + 1 < argc)
            startWord = static_cast<uint32_t>(
                std::stoul(argv[++i], nullptr, 0));
        else if (a == "--count" && i + 1 < argc)
            wordCount = static_cast<uint32_t>(
                std::stoul(argv[++i], nullptr, 0));
    }

    std::printf("=== FTDI Internal EEPROM Dump (FT_ReadEE) ===\n");
    std::printf("  Channel index : %d\n", channelIndex);
    std::printf("  Word range    : 0x%03X – 0x%03X  (%u words)\n",
                startWord, startWord + wordCount - 1, wordCount);
    std::printf("\n");

    try {
        d2xx::MpsseDevice dev(channelIndex, 1'000'000);
        d2xx::SpiMaster   spi(dev);    // not used here, but Eeprom constructor accepts it
        d2xx::Eeprom      eeprom(dev); // internal-EEPROM-only constructor

        const auto words = eeprom.readFtdiInternal(startWord, wordCount);
        hexDump(words, startWord);

        // Decode standard fields when reading from word 0
        if (startWord == 0 && wordCount >= 3) {
            std::printf("\n  Decoded header (standard FTDI layout):\n");
            std::printf("    Word[0x00]  Config / device type : 0x%04X\n", words[0]);
            std::printf("    Word[0x01]  VID                  : 0x%04X%s\n",
                        words[1],
                        words[1] == 0x0403u ? "  (FTDI default)" : "");
            std::printf("    Word[0x02]  PID                  : 0x%04X%s\n",
                        words[2],
                        words[2] == 0x6011u ? "  (FT4232H default)"
                      : words[2] == 0x6010u ? "  (FT2232H default)"
                      : "");
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
