// test_loopback.cpp
//
// Tests MPSSE internal loopback: the FTDI chip connects DO to DI internally
// (command 0x84).  No external wiring is required.
//
// Test 1: Send 256 bytes (0x00..0xFF) and verify they are received identically.
// Test 2: Send 64 bytes of pseudo-random data and verify.
// Test 3: Send a single 0x00 byte — sanity check.
//
// Usage:  test_loopback [channelIndex]
//   channelIndex  D2XX device list index (default: 1 = FT4232H CH-B)

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <random>
#include <vector>

#include "d2xx_mpsse/MpsseDevice.hpp"
#include "d2xx_mpsse/SpiMaster.hpp"

// ---------------------------------------------------------------------------
static bool runTest(d2xx::SpiMaster& spi,
                    const std::vector<uint8_t>& tx,
                    const char* name) {
    std::vector<uint8_t> rx(tx.size(), 0x00u);

    // No CS toggling needed for loopback (no external device)
    spi.transfer(tx.data(), rx.data(), tx.size());

    if (tx == rx) {
        std::printf("[PASS] %s (%zu bytes)\n", name, tx.size());
        return true;
    }

    std::printf("[FAIL] %s (%zu bytes)\n", name, tx.size());
    int shown = 0;
    for (std::size_t i = 0; i < tx.size() && shown < 16; ++i) {
        if (tx[i] != rx[i]) {
            std::printf("       Byte[%zu]: TX=0x%02X  RX=0x%02X\n",
                        i, tx[i], rx[i]);
            ++shown;
        }
    }
    if (shown == 16) std::printf("       ... (more mismatches omitted)\n");
    return false;
}

// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    const int channelIndex = (argc > 1) ? std::atoi(argv[1]) : 1;

    std::printf("=== MPSSE Internal Loopback Test ===\n");
    std::printf("Channel index: %d\n\n", channelIndex);

    try {
        d2xx::MpsseDevice dev(channelIndex, 1'000'000);
        d2xx::SpiMaster   spi(dev);

        spi.enableLoopback(true);
        std::printf("Internal loopback enabled (command 0x84).\n\n");

        bool allPassed = true;

        // --- Test 1: sequential 0x00..0xFF ---
        {
            std::vector<uint8_t> tx(256);
            for (int i = 0; i < 256; ++i)
                tx[static_cast<std::size_t>(i)] = static_cast<uint8_t>(i);
            allPassed &= runTest(spi, tx, "Sequential 0x00-0xFF (256 B)");
        }

        // --- Test 2: pseudo-random 64 bytes (fixed seed for repeatability) ---
        {
            std::mt19937 rng(0xDEAD'BEEF);
            std::vector<uint8_t> tx(64);
            std::generate(tx.begin(), tx.end(),
                          [&] { return static_cast<uint8_t>(rng() & 0xFFu); });
            allPassed &= runTest(spi, tx, "Pseudo-random (64 B, seed=0xDEADBEEF)");
        }

        // --- Test 3: single zero byte ---
        {
            const std::vector<uint8_t> tx = { 0x00u };
            allPassed &= runTest(spi, tx, "Single byte 0x00");
        }

        // --- Test 4: all-0xFF ---
        {
            std::vector<uint8_t> tx(128, 0xFFu);
            allPassed &= runTest(spi, tx, "All-0xFF (128 B)");
        }

        spi.enableLoopback(false);
        std::printf("\nInternal loopback disabled (command 0x85).\n");
        std::printf("\n%s\n", allPassed ? "All tests PASSED." : "Some tests FAILED.");

        return allPassed ? 0 : 1;

    } catch (const d2xx::MpsseError& e) {
        std::fprintf(stderr, "[ERROR] %s  (FT_STATUS=%u)\n",
                     e.what(), e.status());
        return 1;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[ERROR] %s\n", e.what());
        return 1;
    }
}
