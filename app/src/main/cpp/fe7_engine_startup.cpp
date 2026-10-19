#include "fe7_engine_startup.h"

#include "gba_header_parser.h"
#include "gba_port.h"

#include <cstddef>
#include <cstdint>

namespace {
Fe7EngineBootState g_state = FE7_ENGINE_BOOT_NO_ROM;
const char* g_message = "No se ha preparado una ROM para el runtime nativo.";
}

extern "C" Fe7EngineBootState fe7_engine_prepare(const std::uint8_t* rom, std::size_t rom_size) {
    fe7_gba_detach_rom();
    fe7_gba_reset(1);

    fe7::GbaHeader header;
    if (rom == nullptr || rom_size < 0xC0 ||
        !fe7::ParseGbaHeader(rom, rom_size, &header) ||
        !header.looks_like_fire_emblem) {
        g_state = FE7_ENGINE_BOOT_INVALID_ROM;
        g_message = "La ROM no supera la validacion basica del encabezado de Fire Emblem.";
        return g_state;
    }

    if (!fe7_gba_attach_rom(rom, rom_size)) {
        g_state = FE7_ENGINE_BOOT_INVALID_ROM;
        g_message = "No se pudo asociar la ROM al bus de memoria GBA.";
        return g_state;
    }

    g_state = FE7_ENGINE_BOOT_RUNTIME_PREPARED;
    g_message = "ROM asociada y memoria GBA reiniciada; AgbMain todavia no esta enlazado.";
    return g_state;
}

extern "C" void fe7_engine_shutdown(void) {
    fe7_gba_detach_rom();
    fe7_gba_reset(1);
    g_state = FE7_ENGINE_BOOT_NO_ROM;
    g_message = "Runtime nativo detenido.";
}

extern "C" Fe7EngineBootState fe7_engine_boot_state(void) {
    return g_state;
}

extern "C" const char* fe7_engine_boot_message(void) {
    return g_message;
}
