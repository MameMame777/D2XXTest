// test_open.cpp
//
// Tests:
//   1. List all detected D2XX devices.
//   2. Open the specified channel (default: index 1 = FT4232H Channel B).
//   3. Confirm MPSSE sync (performed inside MpsseDevice constructor).
//   4. Print handle address, then close (RAII destructor).
//
// Usage:  test_open [channelIndex]
//   channelIndex  Index in D2XX device list (default: 1 = CH-B for FT4232H)

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>

#include "d2xx_mpsse/MpsseDevice.hpp"

int main(int argc, char* argv[]) {
    const int channelIndex = (argc > 1) ? std::atoi(argv[1]) : 1;

    std::printf("=== D2XX Device List ===\n");
    d2xx::MpsseDevice::listDevices();
    std::printf("\n");

    std::printf("=== Opening channel index %d (MPSSE, 1 MHz) ===\n",
                channelIndex);
    try {
        d2xx::MpsseDevice dev(channelIndex, 1'000'000);

        std::printf("[OK] FT_Open succeeded.\n");
        std::printf("[OK] MPSSE sync successful (0xFA 0xAA + 0xFA 0xAB received).\n");
        std::printf("     FT_HANDLE = %p\n", dev.handle());

        // Destructor closes the device here.
    } catch (const d2xx::MpsseError& e) {
        std::fprintf(stderr, "[FAIL] %s  (FT_STATUS=%u)\n",
                     e.what(), e.status());
        return 1;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[FAIL] %s\n", e.what());
        return 1;
    }

    std::printf("[OK] FT_Close succeeded (RAII).\n");
    return 0;
}
