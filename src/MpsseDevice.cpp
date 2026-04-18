#include "d2xx_mpsse/MpsseDevice.hpp"

#include <ftd2xx.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

namespace d2xx {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------
namespace {

const char* ftStatusStr(FT_STATUS s) noexcept {
    switch (s) {
    case FT_OK:                          return "FT_OK";
    case FT_INVALID_HANDLE:              return "FT_INVALID_HANDLE";
    case FT_DEVICE_NOT_FOUND:            return "FT_DEVICE_NOT_FOUND";
    case FT_DEVICE_NOT_OPENED:           return "FT_DEVICE_NOT_OPENED";
    case FT_IO_ERROR:                    return "FT_IO_ERROR";
    case FT_INSUFFICIENT_RESOURCES:      return "FT_INSUFFICIENT_RESOURCES";
    case FT_INVALID_PARAMETER:           return "FT_INVALID_PARAMETER";
    case FT_INVALID_BAUD_RATE:           return "FT_INVALID_BAUD_RATE";
    case FT_DEVICE_NOT_OPENED_FOR_ERASE: return "FT_DEVICE_NOT_OPENED_FOR_ERASE";
    case FT_DEVICE_NOT_OPENED_FOR_WRITE: return "FT_DEVICE_NOT_OPENED_FOR_WRITE";
    case FT_FAILED_TO_WRITE_DEVICE:      return "FT_FAILED_TO_WRITE_DEVICE";
    case FT_EEPROM_READ_FAILED:          return "FT_EEPROM_READ_FAILED";
    case FT_EEPROM_WRITE_FAILED:         return "FT_EEPROM_WRITE_FAILED";
    case FT_EEPROM_ERASE_FAILED:         return "FT_EEPROM_ERASE_FAILED";
    case FT_EEPROM_NOT_PRESENT:          return "FT_EEPROM_NOT_PRESENT";
    case FT_EEPROM_NOT_PROGRAMMED:       return "FT_EEPROM_NOT_PROGRAMMED";
    case FT_INVALID_ARGS:                return "FT_INVALID_ARGS";
    case FT_NOT_SUPPORTED:               return "FT_NOT_SUPPORTED";
    case FT_OTHER_ERROR:                 return "FT_OTHER_ERROR";
    default:                             return "FT_UNKNOWN_ERROR";
    }
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// MpsseDevice implementation
// ---------------------------------------------------------------------------

void MpsseDevice::checkStatus(unsigned st, const char* ctx) {
    if (st != FT_OK) {
        throw MpsseError(
            std::string(ctx) + " failed: " +
            ftStatusStr(static_cast<FT_STATUS>(st)),
            st);
    }
}

MpsseDevice::MpsseDevice(int channelIndex, uint32_t clockHz) {
    FT_HANDLE h = nullptr;
    checkStatus(
        static_cast<unsigned>(FT_Open(channelIndex, &h)),
        "FT_Open");
    handle_ = h;

    try {
        initMpsse(clockHz);
    } catch (...) {
        FT_Close(h);
        handle_ = nullptr;
        throw;
    }
}

MpsseDevice::~MpsseDevice() noexcept {
    if (handle_) {
        auto* h = static_cast<FT_HANDLE>(handle_);
        FT_SetBitMode(h, 0x00, 0x00); // reset MPSSE → serial mode
        FT_Close(h);
        handle_ = nullptr;
    }
}

MpsseDevice::MpsseDevice(MpsseDevice&& other) noexcept
    : handle_(other.handle_) {
    other.handle_ = nullptr;
}

MpsseDevice& MpsseDevice::operator=(MpsseDevice&& other) noexcept {
    if (this != &other) {
        this->~MpsseDevice();
        handle_       = other.handle_;
        other.handle_ = nullptr;
    }
    return *this;
}

// ---------------------------------------------------------------------------
void MpsseDevice::initMpsse(uint32_t clockHz) {
    auto* h = static_cast<FT_HANDLE>(handle_);

    // USB transfer buffer sizes (bytes): IN transfer = 64 KB, OUT transfer = 64 KB
    checkStatus(
        static_cast<unsigned>(FT_SetUSBParameters(h, 65536, 65536)),
        "FT_SetUSBParameters");

    // Disable event and error characters
    checkStatus(
        static_cast<unsigned>(FT_SetChars(h, 0, 0, 0, 0)),
        "FT_SetChars");

    // Read / write timeouts (ms)
    checkStatus(
        static_cast<unsigned>(FT_SetTimeouts(h, 3000, 3000)),
        "FT_SetTimeouts");

    // Latency timer: 1 ms minimises read latency
    checkStatus(
        static_cast<unsigned>(FT_SetLatencyTimer(h, 1)),
        "FT_SetLatencyTimer");

    // 1. Reset bit mode → normal serial
    checkStatus(
        static_cast<unsigned>(FT_SetBitMode(h, 0x00, 0x00)),
        "FT_SetBitMode(reset)");
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    // 2. Enable MPSSE mode
    checkStatus(
        static_cast<unsigned>(FT_SetBitMode(h, 0x00, 0x02)),
        "FT_SetBitMode(MPSSE)");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Purge stale data
    checkStatus(
        static_cast<unsigned>(FT_Purge(h, FT_PURGE_RX | FT_PURGE_TX)),
        "FT_Purge");

    // Verify that the MPSSE engine is responding
    syncMpsse();

    // Clock configuration:
    //   0x8A  Disable divide-by-5  → 60 MHz base clock
    //   0x97  Disable adaptive clocking (not needed for SPI)
    //   0x8D  Disable 3-phase data clocking (not needed for SPI)
    const uint8_t clockCfg[] = { 0x8A, 0x97, 0x8D };
    write(std::span<const uint8_t>(clockCfg));

    // Set clock divisor:  SCK = 60 MHz / (2 × (1 + divisor))
    // → divisor = (30 000 000 / clockHz) − 1
    const uint32_t divisor = (30'000'000u / clockHz) - 1u;
    const uint8_t clkDiv[] = {
        0x86,
        static_cast<uint8_t>(divisor & 0xFFu),
        static_cast<uint8_t>((divisor >> 8u) & 0xFFu),
    };
    write(std::span<const uint8_t>(clkDiv));

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

// ---------------------------------------------------------------------------
void MpsseDevice::syncMpsse() {
    auto* h = static_cast<FT_HANDLE>(handle_);

    // Helper lambda: send one bad-command byte and verify the 2-byte echo reply.
    auto doSync = [&](uint8_t badCmd) {
        // Clear the queue before each sync attempt
        FT_Purge(h, FT_PURGE_RX | FT_PURGE_TX);

        DWORD written = 0;
        checkStatus(
            static_cast<unsigned>(
                FT_Write(h, static_cast<LPVOID>(&badCmd), 1, &written)),
            "FT_Write(MPSSE sync)");

        // Poll for 2-byte response with 3-second timeout.
        auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(3000);
        uint8_t resp[2] = {};
        std::size_t got = 0;

        while (got < 2) {
            DWORD avail = 0;
            FT_GetQueueStatus(h, &avail);

            if (avail > 0) {
                const DWORD toRead =
                    static_cast<DWORD>(std::min<std::size_t>(avail, 2 - got));
                DWORD rd = 0;
                FT_Read(h, resp + got, toRead, &rd);
                got += rd;
            }

            if (got < 2) {
                if (std::chrono::steady_clock::now() >= deadline) {
                    throw MpsseError(
                        std::string("MPSSE sync timeout (sent 0x") +
                        std::to_string(badCmd) + "); "
                        "check FT_SetBitMode and latency timer.");
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        if (resp[0] != 0xFA || resp[1] != badCmd) {
            throw MpsseError(
                std::string("MPSSE sync failed (sent 0x") +
                std::to_string(badCmd) +
                "): expected [FA " + std::to_string(badCmd) +
                "], got [" + std::to_string(resp[0]) +
                " " + std::to_string(resp[1]) + "]");
        }
    };

    doSync(0xAA);
    doSync(0xAB);
}

// ---------------------------------------------------------------------------
void MpsseDevice::write(std::span<const uint8_t> data) {
    if (data.empty()) return;

    auto* h = static_cast<FT_HANDLE>(handle_);

    // FT_Write requires a non-const void* buffer; the function only reads data.
    DWORD written = 0;
    checkStatus(
        static_cast<unsigned>(FT_Write(
            h,
            static_cast<LPVOID>(const_cast<uint8_t*>(data.data())),
            static_cast<DWORD>(data.size()),
            &written)),
        "FT_Write");

    if (written != static_cast<DWORD>(data.size())) {
        throw MpsseError(
            "FT_Write: short write (requested=" +
            std::to_string(data.size()) + ", written=" +
            std::to_string(written) + ")");
    }
}

// ---------------------------------------------------------------------------
std::vector<uint8_t> MpsseDevice::read(size_t count, uint32_t timeoutMs) {
    if (count == 0) return {};

    auto* h = static_cast<FT_HANDLE>(handle_);

    std::vector<uint8_t> result(count);
    std::size_t received = 0;

    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(timeoutMs);

    while (received < count) {
        DWORD avail = 0;
        FT_GetQueueStatus(h, &avail);

        if (avail > 0) {
            const DWORD toRead =
                static_cast<DWORD>(std::min<std::size_t>(avail, count - received));
            DWORD rd = 0;
            checkStatus(
                static_cast<unsigned>(
                    FT_Read(h, result.data() + received, toRead, &rd)),
                "FT_Read");
            received += rd;
        }

        if (received < count) {
            if (std::chrono::steady_clock::now() >= deadline) {
                throw MpsseError(
                    "FT_Read timeout: expected=" + std::to_string(count) +
                    " bytes, received=" + std::to_string(received));
            }
            if (avail == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
void MpsseDevice::flush() {
    const uint8_t cmd = 0x87u; // Send Immediate
    write(std::span<const uint8_t>(&cmd, 1));
}

// ---------------------------------------------------------------------------
uint8_t MpsseDevice::readPins(bool highByte) {
    const uint8_t cmd[2] = { static_cast<uint8_t>(highByte ? 0x83u : 0x81u), 0x87u };
    write(std::span<const uint8_t>(cmd));
    return read(1)[0];
}

// ---------------------------------------------------------------------------
uint8_t MpsseDevice::getBitMode() {
    auto* h = static_cast<FT_HANDLE>(handle_);
    UCHAR mode = 0;
    checkStatus(
        static_cast<unsigned>(FT_GetBitMode(h, &mode)),
        "FT_GetBitMode");
    return static_cast<uint8_t>(mode);
}

// ---------------------------------------------------------------------------
void MpsseDevice::listDevices() {
    DWORD numDevs = 0;
    FT_STATUS st  = FT_CreateDeviceInfoList(&numDevs);

    if (st != FT_OK) {
        std::printf("FT_CreateDeviceInfoList failed: %s\n",
                    ftStatusStr(st));
        return;
    }

    if (numDevs == 0) {
        std::printf("No D2XX devices found.\n");
        return;
    }

    std::vector<FT_DEVICE_LIST_INFO_NODE> info(numDevs);
    st = FT_GetDeviceInfoList(info.data(), &numDevs);
    if (st != FT_OK) {
        std::printf("FT_GetDeviceInfoList failed: %s\n", ftStatusStr(st));
        return;
    }

    std::printf("D2XX devices found: %u\n", static_cast<unsigned>(numDevs));
    for (DWORD i = 0; i < numDevs; ++i) {
        const auto& d = info[i];
        std::printf("  [%u] Flags=0x%02X  Type=%-2u  ID=0x%08X  "
                    "LocID=0x%08X  SerialNo=%-16s  Desc=%s\n",
                    static_cast<unsigned>(i),
                    static_cast<unsigned>(d.Flags),
                    static_cast<unsigned>(d.Type),
                    static_cast<unsigned>(d.ID),
                    static_cast<unsigned>(d.LocId),
                    d.SerialNumber,
                    d.Description);
    }
}

} // namespace d2xx
