#include <jni.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <mutex>
#include <cstring>
#include <string>
#include <vector>

#include "fe7_engine_startup.h"
#include "gba_header_parser.h"
#include "gba_port.h"
#include "third_party/libretro.h"

namespace {
constexpr unsigned kFrameWidth = 240;
constexpr unsigned kFrameHeight = 160;
std::vector<std::uint32_t> g_frame;
std::vector<unsigned char> g_rom;
bool g_initialized = false;
bool g_loaded = false;
unsigned g_width = kFrameWidth;
unsigned g_height = kFrameHeight;
unsigned g_pixel_format = RETRO_PIXEL_FORMAT_XRGB8888;
unsigned g_buttons = 0;

// Bounded stereo PCM queue shared by the emulation and Android audio threads.
constexpr std::size_t kAudioQueueFrames = 8192; // 250 ms maximum at 32,768 Hz
std::int16_t g_audio_queue[kAudioQueueFrames * 2]{};
std::size_t g_audio_read = 0;
std::size_t g_audio_write = 0;
std::size_t g_audio_count = 0;
std::mutex g_audio_mutex;

void QueueAudio(const std::int16_t* samples, std::size_t frames) {
    if (samples == nullptr || frames == 0) return;
    std::lock_guard<std::mutex> lock(g_audio_mutex);
    // If the consumer falls behind, discard the oldest samples to keep audio live.
    if (frames >= kAudioQueueFrames) {
        samples += (frames - kAudioQueueFrames) * 2;
        frames = kAudioQueueFrames;
        g_audio_read = g_audio_write = g_audio_count = 0;
    }
    while (g_audio_count + frames > kAudioQueueFrames) {
        g_audio_read = (g_audio_read + 1) % kAudioQueueFrames;
        --g_audio_count;
    }
    for (std::size_t i = 0; i < frames; ++i) {
        g_audio_queue[g_audio_write * 2] = samples[i * 2];
        g_audio_queue[g_audio_write * 2 + 1] = samples[i * 2 + 1];
        g_audio_write = (g_audio_write + 1) % kAudioQueueFrames;
        ++g_audio_count;
    }
}

std::size_t ReadAudio(std::int16_t* output, std::size_t capacity_frames) {
    if (output == nullptr || capacity_frames == 0) return 0;
    std::lock_guard<std::mutex> lock(g_audio_mutex);
    const std::size_t frames = std::min(capacity_frames, g_audio_count);
    for (std::size_t i = 0; i < frames; ++i) {
        output[i * 2] = g_audio_queue[g_audio_read * 2];
        output[i * 2 + 1] = g_audio_queue[g_audio_read * 2 + 1];
        g_audio_read = (g_audio_read + 1) % kAudioQueueFrames;
    }
    g_audio_count -= frames;
    return frames;
}

void ClearAudio() {
    std::lock_guard<std::mutex> lock(g_audio_mutex);
    g_audio_read = g_audio_write = g_audio_count = 0;
}

bool Environment(unsigned command, void* data) {
    switch (command) {
        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
            if (data == nullptr) return false;
            g_pixel_format = *static_cast<enum retro_pixel_format*>(data);
            return g_pixel_format == RETRO_PIXEL_FORMAT_XRGB8888 ||
                   g_pixel_format == RETRO_PIXEL_FORMAT_RGB565;
        case RETRO_ENVIRONMENT_GET_CAN_DUPE:
            if (data != nullptr) {
                *static_cast<bool*>(data) = true;
                return true;
            }
            return false;
        case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
            return true;
        default:
            return false;
    }
}

void VideoRefresh(const void* data, unsigned width, unsigned height, size_t pitch) {
    if (data == nullptr) return;
    if (width == 0 || height == 0 || width > 512 || height > 512) return;
    g_width = width;
    g_height = height;
    g_frame.resize(static_cast<std::size_t>(width) * height);
    const auto* row = static_cast<const unsigned char*>(data);

    for (unsigned y = 0; y < height; ++y) {
        if (g_pixel_format == RETRO_PIXEL_FORMAT_RGB565) {
            const auto* source = reinterpret_cast<const std::uint16_t*>(row + y * pitch);
            for (unsigned x = 0; x < width; ++x) {
                const std::uint16_t p = source[x];
                const std::uint32_t r = ((p >> 11) & 0x1F) * 255 / 31;
                const std::uint32_t g = ((p >> 5) & 0x3F) * 255 / 63;
                const std::uint32_t b = (p & 0x1F) * 255 / 31;
                g_frame[static_cast<std::size_t>(y) * width + x] =
                    0xFF000000u | (r << 16) | (g << 8) | b;
            }
        } else {
            const auto* source = reinterpret_cast<const std::uint32_t*>(row + y * pitch);
            for (unsigned x = 0; x < width; ++x) {
                g_frame[static_cast<std::size_t>(y) * width + x] =
                    0xFF000000u | (source[x] & 0x00FFFFFFu);
            }
        }
    }
}

void AudioSample(std::int16_t left, std::int16_t right) {
    const std::int16_t sample[2] = {left, right};
    QueueAudio(sample, 1);
}
size_t AudioSampleBatch(const std::int16_t* data, size_t frames) {
    QueueAudio(data, frames);
    return frames;
}
void InputPoll() {}
std::int16_t InputState(unsigned port, unsigned device, unsigned index, unsigned id) {
    if (port != 0 || device != RETRO_DEVICE_JOYPAD || index != 0 || id >= 32) return 0;
    return (g_buttons & (1u << id)) != 0 ? 1 : 0;
}

void StopCore() {
    if (g_loaded) {
        retro_unload_game();
        g_loaded = false;
    }
    if (g_initialized) {
        retro_deinit();
        g_initialized = false;
    }
    // The software GBA bus borrows g_rom.data(); detach it before the vector
    // can release or reallocate its backing storage.
    fe7_engine_shutdown();
    g_rom.clear();
    g_frame.clear();
    g_width = kFrameWidth;
    g_height = kFrameHeight;
    g_buttons = 0;
    ClearAudio();
}

jstring NewError(JNIEnv* env, const std::string& message) {
    return env->NewStringUTF(message.c_str());
}

jintArray CopyFrame(JNIEnv* env) {
    if (g_frame.empty()) return env->NewIntArray(0);
    const std::size_t count = static_cast<std::size_t>(g_width) * g_height;
    jintArray result = env->NewIntArray(static_cast<jsize>(count));
    if (result == nullptr) return nullptr;
    std::vector<jint> output(count);
    for (std::size_t i = 0; i < count; ++i) output[i] = static_cast<jint>(g_frame[i]);
    env->SetIntArrayRegion(result, 0, static_cast<jsize>(count), output.data());
    return result;
}
}  // namespace

extern "C" JNIEXPORT jstring JNICALL
Java_org_fe7android_nativeport_MainActivity_nativeGetBridgeStatus(JNIEnv* env, jobject) {
    const std::string status =
        "Puente C++/JNI activo; el juego sigue ejecutándose en mGBA/libretro. "
        "El runtime FE7 nativo prepara memoria/ROM, pero aún no ejecuta AgbMain.";
    return env->NewStringUTF(status.c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_fe7android_nativeport_MainActivity_nativeInspectGbaHeader(
    JNIEnv* env, jobject, jbyteArray header_array) {
    if (header_array == nullptr) return env->NewStringUTF("\n\n0");
    const jsize length = env->GetArrayLength(header_array);
    if (length < static_cast<jsize>(fe7::kGbaHeaderRequiredSize)) {
        return env->NewStringUTF("\n\n0");
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    env->GetByteArrayRegion(header_array, 0, length,
                            reinterpret_cast<jbyte*>(bytes.data()));
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return env->NewStringUTF("\n\n0");
    }
    fe7::GbaHeader header;
    if (!fe7::ParseGbaHeader(bytes.data(), bytes.size(), &header)) {
        return env->NewStringUTF("\n\n0");
    }
    const std::string result = header.title + "\n" + header.game_code + "\n" +
        (header.looks_like_fire_emblem ? "1" : "0");
    return env->NewStringUTF(result.c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_fe7android_nativeport_MainActivity_nativeStartGame(JNIEnv* env, jobject, jstring path_value) {
    if (path_value == nullptr) return NewError(env, "No se recibió la ruta de la ROM.");
    const char* path = env->GetStringUTFChars(path_value, nullptr);
    if (path == nullptr) return NewError(env, "No se pudo leer la ruta de la ROM.");
    const std::string path_string(path);
    env->ReleaseStringUTFChars(path_value, path);

    StopCore();
    std::ifstream file(path_string, std::ios::binary);
    if (!file) return NewError(env, "Android no pudo abrir la copia local de la ROM.");
    g_rom.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    if (g_rom.size() < 0xC0) {
        g_rom.clear();
        return NewError(env, "La ROM es demasiado pequeña para ser una ROM GBA válida.");
    }

    // Attach the same user-supplied ROM buffer to the native GBA memory bus.
    // This is preparation/integration only: mGBA remains responsible for all
    // actual CPU execution and gameplay until the FE7 engine can be linked.
    const Fe7EngineBootState native_state =
        fe7_engine_prepare(g_rom.data(), g_rom.size());
    if (native_state != FE7_ENGINE_BOOT_RUNTIME_PREPARED) {
        const char* native_message = fe7_engine_boot_message();
        const std::string error = native_message != nullptr
            ? native_message : "motivo desconocido";
        StopCore();
        return NewError(env, "El runtime nativo rechazó la ROM: " + error);
    }

    retro_set_environment(Environment);
    retro_set_video_refresh(VideoRefresh);
    retro_set_audio_sample(AudioSample);
    retro_set_audio_sample_batch(AudioSampleBatch);
    retro_set_input_poll(InputPoll);
    retro_set_input_state(InputState);
    retro_init();
    g_initialized = true;

    retro_system_info info{};
    retro_get_system_info(&info);
    retro_game_info game{};
    game.path = path_string.c_str();
    game.data = info.need_fullpath ? nullptr : g_rom.data();
    game.size = info.need_fullpath ? 0 : g_rom.size();
    game.meta = nullptr;

    if (!retro_load_game(&game)) {
        StopCore();
        return NewError(env, "mGBA no pudo cargar el archivo. Comprueba que sea una ROM GBA válida.");
    }
    g_loaded = true;
    retro_system_av_info av{};
    retro_get_system_av_info(&av);
    if (av.geometry.base_width > 0 && av.geometry.base_width <= 512) g_width = av.geometry.base_width;
    if (av.geometry.base_height > 0 && av.geometry.base_height <= 512) g_height = av.geometry.base_height;

    // A first frame gives the frontend a real buffer before the animation loop.
    retro_run();
    return NewError(env, "");
}

extern "C" JNIEXPORT jintArray JNICALL
Java_org_fe7android_nativeport_MainActivity_nativeRunFrame(JNIEnv* env, jobject) {
    if (g_loaded) retro_run();
    return CopyFrame(env);
}

/** Run one emulation frame directly into a reusable Java direct buffer.
 *  This avoids allocating and converting a new jint[] on every frame.
 */
extern "C" JNIEXPORT jboolean JNICALL
Java_org_fe7android_nativeport_MainActivity_nativeRunFrameInto(
    JNIEnv* env, jobject, jobject output_buffer) {
    if (output_buffer == nullptr) return JNI_FALSE;
    auto* output = static_cast<std::uint32_t*>(env->GetDirectBufferAddress(output_buffer));
    const jlong capacity = env->GetDirectBufferCapacity(output_buffer);
    if (output == nullptr || capacity < 0) return JNI_FALSE;
    if (g_loaded) retro_run();
    const std::size_t available = static_cast<std::size_t>(capacity) / sizeof(std::uint32_t);
    const std::size_t count = std::min(available, g_frame.size());
    // Bitmap.copyPixelsFromBuffer consumes RGBA byte order for ARGB_8888
    // storage on Android. g_frame stores 0xAARRGGBB words, whose in-memory
    // byte order on little-endian devices is BGRA, so swap red/blue here.
    // Keep conversion native to avoid per-frame Kotlin allocations.
    for (std::size_t i = 0; i < count; ++i) {
        const std::uint32_t pixel = g_frame[i];
        output[i] = (pixel & 0xFF00FF00u) |
                    ((pixel & 0x00FF0000u) >> 16) |
                    ((pixel & 0x000000FFu) << 16);
    }
    return count == g_frame.size() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_org_fe7android_nativeport_MainActivity_nativeSetButtons(JNIEnv*, jobject, jint buttons) {
    g_buttons = static_cast<unsigned>(buttons);

    // Libretro uses a different button order than GBA KEYINPUT. Preserve
    // g_buttons for mGBA and mirror the same physical presses to the native bus.
    std::uint16_t gba_pressed = 0;
    if (g_buttons & (1u << 8))  gba_pressed |= 1u << 0; // A
    if (g_buttons & (1u << 0))  gba_pressed |= 1u << 1; // B
    if (g_buttons & (1u << 2))  gba_pressed |= 1u << 2; // Select
    if (g_buttons & (1u << 3))  gba_pressed |= 1u << 3; // Start
    if (g_buttons & (1u << 7))  gba_pressed |= 1u << 4; // Right
    if (g_buttons & (1u << 6))  gba_pressed |= 1u << 5; // Left
    if (g_buttons & (1u << 4))  gba_pressed |= 1u << 6; // Up
    if (g_buttons & (1u << 5))  gba_pressed |= 1u << 7; // Down
    if (g_buttons & (1u << 11)) gba_pressed |= 1u << 8; // R
    if (g_buttons & (1u << 10)) gba_pressed |= 1u << 9; // L
    fe7_gba_set_keys(gba_pressed);
}

extern "C" JNIEXPORT void JNICALL
Java_org_fe7android_nativeport_MainActivity_nativeStopGame(JNIEnv*, jobject) {
    StopCore();
}

/** Keep the standalone JNI/video-path diagnostic available beside the real core. */
extern "C" JNIEXPORT jintArray JNICALL
Java_org_fe7android_nativeport_MainActivity_nativeCreateTestFrame(JNIEnv* env, jobject) {
    // Exercise the real software GBA PPU against synthetic VRAM/palette/OAM data.
    // This diagnostic does not load or pretend to boot an FE7 ROM.
    fe7_gba_reset(1);

    auto* display_control = static_cast<volatile std::uint16_t*>(
        fe7_gba_io_pointer(0x04000000u, sizeof(std::uint16_t)));
    auto* bg0_control = static_cast<volatile std::uint16_t*>(
        fe7_gba_io_pointer(0x04000008u, sizeof(std::uint16_t)));
    auto* palette = static_cast<std::uint16_t*>(
        fe7_gba_resolve_pointer(0x05000000u, 0x400u, 1));
    auto* vram = static_cast<std::uint8_t*>(
        fe7_gba_resolve_pointer(0x06000000u, 0x18000u, 1));
    auto* oam = static_cast<std::uint16_t*>(
        fe7_gba_resolve_pointer(0x07000000u, 0x400u, 1));
    if (display_control == nullptr || bg0_control == nullptr ||
        palette == nullptr || vram == nullptr || oam == nullptr) {
        return env->NewIntArray(0);
    }

    *display_control = static_cast<std::uint16_t>((1u << 8) | (1u << 6) | (1u << 12));
    *bg0_control = static_cast<std::uint16_t>(31u << 8);  // BG0 screen base block 31.

    palette[1] = 0x001Fu;       // BG palette 1: red.
    palette[2] = 0x7C00u;       // BG palette 2: blue.
    palette[256u + 3u] = 0x03E0u;  // OBJ palette 3: green.

    // Two synthetic 4bpp character tiles, addressed in 32-byte units.
    std::fill(vram, vram + 32, 0x11u);
    std::fill(vram + 32, vram + 64, 0x22u);
    // OBJ tile memory starts at VRAM + 0x10000.
    std::fill(vram + 0x10000u, vram + 0x10000u + 32u, 0x33u);

    // Checker pattern across the 32x32 text map in screen block 31.
    auto* tile_map = reinterpret_cast<std::uint16_t*>(vram + 31u * 0x800u);
    for (unsigned y = 0; y < 32; ++y) {
        for (unsigned x = 0; x < 32; ++x) {
            tile_map[y * 32u + x] = static_cast<std::uint16_t>(((x / 4u + y / 4u) & 1u) ? 1u : 0u);
        }
    }

    // Hide unused objects; draw one 8x8 green sprite at (90, 64).
    std::fill(oam, oam + 128u * 4u, 0u);
    for (unsigned i = 0; i < 128; ++i) oam[i * 4u] = 0x0200u;
    oam[0] = 64u;   // Attribute 0: Y, regular square OBJ.
    oam[1] = 90u;   // Attribute 1: X, 8x8 size.
    oam[2] = 0u;    // Attribute 2: tile 0, priority 0, palette bank 0.

    std::vector<std::uint32_t> pixels(static_cast<std::size_t>(kFrameWidth) * kFrameHeight);
    if (!fe7_gba_render_frame(pixels.data(), pixels.size())) {
        return env->NewIntArray(0);
    }
    jintArray result = env->NewIntArray(static_cast<jsize>(pixels.size()));
    if (result == nullptr) return nullptr;
    env->SetIntArrayRegion(result, 0, static_cast<jsize>(pixels.size()),
                           reinterpret_cast<const jint*>(pixels.data()));
    return result;
}

extern "C" JNIEXPORT jint JNICALL
Java_org_fe7android_nativeport_MainActivity_nativeReadAudio(
    JNIEnv* env, jobject, jshortArray output_array, jint max_frames) {
    if (output_array == nullptr || max_frames <= 0) return 0;
    const jsize capacity = env->GetArrayLength(output_array);
    const std::size_t frames = std::min(
        static_cast<std::size_t>(max_frames),
        static_cast<std::size_t>(capacity) / 2);
    if (frames == 0) return 0;
    // Avoid allocating a temporary vector on every audio pull.
    std::int16_t samples[2048 * 2];
    const std::size_t safe_frames = std::min(frames, static_cast<std::size_t>(2048));
    const std::size_t count = ReadAudio(samples, safe_frames);
    if (count > 0) {
        env->SetShortArrayRegion(output_array, 0, static_cast<jsize>(count * 2),
                                 reinterpret_cast<const jshort*>(samples));
    }
    return static_cast<jint>(count);
}
