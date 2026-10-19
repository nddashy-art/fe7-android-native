#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

void fe7_gba_set_irq_handler(unsigned index, void (*handler)(void));
void fe7_gba_irq_clear(uint16_t flags);
void fe7_gba_irq_raise(uint16_t flags);
void fe7_gba_dispatch_pending_irqs(void);

// Native replacements for GBA BIOS sleep/IRQ services.
void IntrWait(int8_t wait_next, uint16_t intr_flags);
void Halt(void);
void Stop(void);

#ifdef __cplusplus
}
#endif
