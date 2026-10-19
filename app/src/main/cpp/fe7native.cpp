#include <jni.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "gba_header_parser.h"
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

void AudioSample(std::int16_t, std::int16_t) {}
size_t AudioSampleBatch(const std::int16_t*, size_t frames) { return frames; }
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
    g_rom.clear();
    g_frame.clear();
    g_width = kFrameWidth;
    g_height = kFrameHeight;
    g_buttons = 0;
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
    return env->NewStringUTF("Puente C++/JNI activo; motor mGBA integrado");
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

extern "C" JNIEXPORT void JNICALL
Java_org_fe7android_nativeport_MainActivity_nativeSetButtons(JNIEnv*, jobject, jint buttons) {
    g_buttons = static_cast<unsigned>(buttons);
}

extern "C" JNIEXPORT void JNICALL
Java_org_fe7android_nativeport_MainActivity_nativeStopGame(JNIEnv*, jobject) {
    StopCore();
}
