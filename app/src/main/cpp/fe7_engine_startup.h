#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum Fe7EngineBootState {
    FE7_ENGINE_BOOT_NO_ROM = 0,
    FE7_ENGINE_BOOT_RUNTIME_PREPARED = 1,
    FE7_ENGINE_BOOT_INVALID_ROM = 2
} Fe7EngineBootState;

// Prepares the software GBA memory/BIOS runtime for a caller-owned ROM.
// This deliberately does not claim the game has started: AgbMain and the
// renderer/audio/timing loop must be linked before that can be true.
Fe7EngineBootState fe7_engine_prepare(const uint8_t* rom, size_t rom_size);
void fe7_engine_shutdown(void);
Fe7EngineBootState fe7_engine_boot_state(void);
const char* fe7_engine_boot_message(void);

#ifdef __cplusplus
}
#endif
