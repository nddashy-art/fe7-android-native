#include "gba_irq.h"
#include "gba_port.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <thread>

namespace {
constexpr unsigned kInterruptCount = 14;
constexpr std::uint16_t kAllInterrupts = 0x3FFFu;
std::array<void (*)(void), kInterruptCount> g_handlers{};

volatile std::uint16_t* Io16(std::uint32_t address) {
    return static_cast<volatile std::uint16_t*>(fe7_gba_io_pointer(address, sizeof(std::uint16_t)));
}
volatile std::uint16_t* InterruptEnable() { return Io16(0x04000200u); }
volatile std::uint16_t* InterruptFlags() { return Io16(0x04000202u); }
volatile std::uint16_t* InterruptMasterEnable() { return Io16(0x04000208u); }
}

extern "C" void fe7_gba_set_irq_handler(unsigned index, void (*handler)(void)) {
    if (index >= kInterruptCount) return;
    g_handlers[index] = handler;
}

extern "C" void fe7_gba_irq_clear(std::uint16_t flags) {
    auto* reg = InterruptFlags();
    if (reg != nullptr) *reg = static_cast<std::uint16_t>(*reg & ~flags);
}

extern "C" void fe7_gba_irq_raise(std::uint16_t flags) {
    auto* reg = InterruptFlags();
    if (reg == nullptr) return;
    *reg = static_cast<std::uint16_t>(*reg | (flags & kAllInterrupts));
    fe7_gba_dispatch_pending_irqs();
}

extern "C" void fe7_gba_dispatch_pending_irqs(void) {
    static bool dispatching = false;
    if (dispatching) return;
    auto* ime = InterruptMasterEnable();
    auto* ie = InterruptEnable();
    auto* flags = InterruptFlags();
    if (ime == nullptr || ie == nullptr || flags == nullptr || ((*ime & 1u) == 0)) return;

    dispatching = true;
    // Recalculate after every callback because a handler may raise another IRQ.
    for (unsigned serviced = 0; serviced < 64; ++serviced) {
        const std::uint16_t pending = static_cast<std::uint16_t>(*ie & *flags & kAllInterrupts);
        if (pending == 0) break;
        unsigned index = 0;
        while (index < kInterruptCount && (pending & (1u << index)) == 0) ++index;
        if (index >= kInterruptCount) break;
        const std::uint16_t bit = static_cast<std::uint16_t>(1u << index);
        auto handler = g_handlers[index];
        if (handler != nullptr) handler();
        // GBA IF is write-one-to-clear. The virtual backing store models the
        // resulting state rather than storing the written acknowledgement mask.
        *flags = static_cast<std::uint16_t>(*flags & ~bit);
    }
    dispatching = false;
}

extern "C" void IntrWait(std::int8_t wait_next, std::uint16_t intr_flags) {
    const std::uint16_t mask = static_cast<std::uint16_t>(intr_flags & kAllInterrupts);
    if (mask == 0) return;
    if (wait_next != 0) fe7_gba_irq_clear(mask);
    auto* flags = InterruptFlags();
    while (flags != nullptr && ((*flags & mask) == 0)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    fe7_gba_dispatch_pending_irqs();
}

extern "C" void Halt(void) { IntrWait(0, kAllInterrupts); }
extern "C" void Stop(void) { IntrWait(0, kAllInterrupts); }
