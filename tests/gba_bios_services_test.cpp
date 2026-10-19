#include "gba_port.h"
#include "gba_memory.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>

namespace {
int g_vblank_calls = 0;
int g_reset_calls = 0;
void VBlankCallback() { ++g_vblank_calls; }
void ResetCallback() { ++g_reset_calls; }
struct BgSrc { std::int32_t tex_x, tex_y; std::int16_t scr_x, scr_y, sx, sy; std::uint16_t alpha; };
struct BgDst { std::int16_t pa, pb, pc, pd; std::int32_t dx, dy; };
struct ObjSrc { std::int16_t x_scale, y_scale; std::uint16_t rotation; };
}

int main() {
    assert(Div(7, 3) == 2);
    assert(Div(-7, 3) == -2);
    assert(DivRem(-7, 3) == -1);
    assert(Div(1, 0) == 0);
    assert(DivRem(1, 0) == 0);
    assert(Sqrt(0) == 0);
    assert(Sqrt(81) == 9);
    assert(Sqrt(0xFFFFFFFFu) == 65535u);
    assert(ArcTan2(1, 0) > 0x3FF0u && ArcTan2(1, 0) < 0x4010u);
    assert(ArcTan2(0, 1) == 0u);

    BgSrc bg_source{0x1000, 0x2000, 0, 0, 0x100, 0x100, 0};
    BgDst bg_destination{};
    BgAffineSet(&bg_source, &bg_destination, 1);
    assert(bg_destination.pa == 0x100 && bg_destination.pb == 0);
    assert(bg_destination.pc == 0 && bg_destination.pd == 0x100);
    assert(bg_destination.dx == bg_source.tex_x && bg_destination.dy == bg_source.tex_y);

    ObjSrc obj_source{0x100, 0x100, 0};
    std::array<std::uint8_t, 32> obj_destination{};
    ObjAffineSet(&obj_source, obj_destination.data(), 1, 8);
    assert(obj_destination[0] == 0x00 && obj_destination[1] == 0x01);
    assert(obj_destination[8] == 0 && obj_destination[9] == 0);
    assert(obj_destination[16] == 0 && obj_destination[17] == 0);
    assert(obj_destination[24] == 0x00 && obj_destination[25] == 0x01);

    fe7_gba_reset(1);
    auto* ewram = static_cast<std::uint8_t*>(fe7_gba_resolve_pointer(0x02000000u, 4, 1));
    auto* sram = static_cast<std::uint8_t*>(fe7_gba_resolve_pointer(0x0E000000u, 4, 1));
    assert(ewram != nullptr && sram != nullptr);
    ewram[0] = 0xAA;
    sram[0] = 0x55;
    RegisterRamReset((1u << 0) | (1u << 7));
    assert(ewram[0] == 0);
    assert(sram[0] == 0x55);
    auto* keyinput = static_cast<volatile std::uint16_t*>(fe7_gba_io_pointer(0x04000130u, 2));
    assert(keyinput != nullptr && *keyinput == 0x03FFu);

    fe7_gba_set_vblank_callback(VBlankCallback);
    VBlankIntrWait();
    VBlankIntrWait();
    assert(g_vblank_calls == 2);
    fe7_gba_set_vblank_callback(nullptr);
    fe7_gba_set_soft_reset_callback(ResetCallback);
    SoftReset(0xFFu);
    assert(g_reset_calls == 1);
    fe7_gba_set_soft_reset_callback(nullptr);

    SoundBiasSet();
    auto* bias = static_cast<volatile std::uint16_t*>(fe7_gba_io_pointer(0x04000088u, 2));
    assert(bias != nullptr && *bias == 0x0200u);
    SoundBiasReset();
    assert(*bias == 0u);
    std::puts("GBA BIOS service tests passed");
    return 0;
}
