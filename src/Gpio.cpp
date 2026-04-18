#include "d2xx_mpsse/Gpio.hpp"

namespace d2xx {

Gpio::Gpio(MpsseDevice& dev, Bank bank)
    : dev_(dev), bank_(bank) {
    // Defer any I/O until the caller calls setDirection() or write().
}

// ---------------------------------------------------------------------------
// Direction
// ---------------------------------------------------------------------------

void Gpio::setDirection(uint8_t dirMask) {
    direction_ = dirMask;
    applyOutput();
}

// ---------------------------------------------------------------------------
// Output
// ---------------------------------------------------------------------------

void Gpio::write(uint8_t value) {
    value_ = value;
    applyOutput();
}

void Gpio::setPin(int pin, bool high) {
    if (high)
        value_ |= static_cast<uint8_t>(1u << pin);
    else
        value_ &= static_cast<uint8_t>(~(1u << pin));
    applyOutput();
}

void Gpio::applyOutput() {
    const uint8_t cmd[3] = { cmdSet(), value_, direction_ };
    dev_.write(std::span<const uint8_t>(cmd));
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

uint8_t Gpio::read() {
    return dev_.readPins(bank_ == Bank::High);
}

bool Gpio::getPin(int pin) {
    return static_cast<bool>((read() >> pin) & 1u);
}

} // namespace d2xx
