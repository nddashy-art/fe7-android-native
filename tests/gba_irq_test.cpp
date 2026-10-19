#include "gba_irq.h"
#include "gba_port.h"

#include <cassert>
#include <cstdint>
#include <cstdio>

namespace {
int g_vblank = 0;
int g_timer = 0;
void OnVBlank() { ++g_vblank; }
void OnTimer() { ++g_timer; }
volatile std::uint16_t* Io16(std::uint32_t address) {
    return static_cast<volatile std::uint16_t*>(fe7_gba_io_pointer(address, 2));
}
}

int main() {
    fe7_gba_reset(1);
    fe7_gba_set_irq_handler(0, OnVBlank);
    fe7_gba_set_irq_handler(3, OnTimer);
    auto* ime = Io16(0x04000208u);
    auto* ie = Io16(0x04000200u);
    auto* flags = Io16(0x04000202u);
    assert(ime && ie && flags);

    *ime = 1;
    *ie = 1u;
    fe7_gba_irq_raise(1u);
    assert(g_vblank == 1 && (*flags & 1u) == 0);

    *ime = 0;
    *ie = 1u << 3;
    fe7_gba_irq_raise(1u << 3);
    assert(g_timer == 0 && (*flags & (1u << 3)) != 0);
    *ime = 1;
    fe7_gba_dispatch_pending_irqs();
    assert(g_timer == 1 && (*flags & (1u << 3)) == 0);

    *ime = 0;
    *ie = 0;
    fe7_gba_irq_raise(1u << 2);
    IntrWait(0, 1u << 2);
    assert((*flags & (1u << 2)) != 0);
    fe7_gba_irq_clear(0x3FFFu);
    assert(*flags == 0);

    fe7_gba_set_irq_handler(14, OnTimer); // invalid index is ignored
    std::puts("GBA IRQ tests passed");
    return 0;
}
