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
}

extern "C" JNIEXPORT void JNICALL
Java_org_fe7android_nativeport_MainActivity_nativeStopGame(JNIEnv*, jobject) {
    StopCore();
}

/** Keep the standalone JNI/video-path diagnostic available beside the real core. */
extern "C" JNIEXPORT jintArray JNICALL
Java_org_fe7android_nativeport_MainActivity_nativeCreateTestFrame(JNIEnv* env, jobject) {
    constexpr int width = 240;
    constexpr int height = 160;
    std::vector<jint> pixels(static_cast<std::size_t>(width * height));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            std::uint32_t color = 0xFF000000u;
            if (y < 105) {
                color |= (static_cast<std::uint32_t>(45 + y / 3) << 16) |
                         (static_cast<std::uint32_t>(105 + y / 5) << 8) |
                         static_cast<std::uint32_t>(165 + y / 8);
            } else if (y < 145) {
                color |= 0xFF3C844Bu;
            } else {
                static const std::uint32_t palette[] = {
                    0xFFDC504Eu, 0xFFF09D49u, 0xFFF5DA62u, 0xFF64BE6Eu,
                    0xFF4AA6C2u, 0xFF6473DCu, 0xFFA667CCu, 0xFFEDEDE4u,
                    0xFF373F4Eu, 0xFF121720u
                };
                color = palette[std::min((x - (x % 22)) / 22, 9)];
            }
            if (x < 2 || x >= width - 2 || y < 2 || y >= height - 2) color = 0xFFFFD98Au;
            pixels[static_cast<std::size_t>(y * width + x)] = static_cast<jint>(color);
        }
    }
    jintArray result = env->NewIntArray(static_cast<jsize>(pixels.size()));
    if (result == nullptr) return nullptr;
    env->SetIntArrayRegion(result, 0, static_cast<jsize>(pixels.size()), pixels.data());
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
