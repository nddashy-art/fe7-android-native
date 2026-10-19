#include <jni.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include "gba_header_parser.h"

extern "C" JNIEXPORT jstring JNICALL
Java_org_fe7android_nativeport_MainActivity_nativeGetBridgeStatus(
    JNIEnv* env, jobject /* thiz */) {
    return env->NewStringUTF(
        "Puente nativo C++ activo; motor original FE7 aún no portado");
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_fe7android_nativeport_MainActivity_nativeInspectGbaHeader(
    JNIEnv* env, jobject /* thiz */, jbyteArray header_array) {
    if (header_array == nullptr) {
        return env->NewStringUTF("\n\n0");
    }

    const jsize length = env->GetArrayLength(header_array);
    if (length < static_cast<jsize>(fe7::kGbaHeaderRequiredSize)) {
        return env->NewStringUTF("\n\n0");
    }

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    env->GetByteArrayRegion(
        header_array, 0, length,
        reinterpret_cast<jbyte*>(bytes.data()));

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return env->NewStringUTF("\n\n0");
    }

    fe7::GbaHeader header;
    if (!fe7::ParseGbaHeader(bytes.data(), bytes.size(), &header)) {
        return env->NewStringUTF("\n\n0");
    }

    const std::string result =
        header.title + "\n" + header.game_code + "\n" +
        (header.looks_like_fire_emblem ? "1" : "0");

    return env->NewStringUTF(result.c_str());
}

namespace {
constexpr int kTestFrameWidth = 240;
constexpr int kTestFrameHeight = 160;

jint TestArgb(int red, int green, int blue) {
    return static_cast<jint>(0xFF000000u |
        (static_cast<std::uint32_t>(red & 0xFF) << 16) |
        (static_cast<std::uint32_t>(green & 0xFF) << 8) |
        static_cast<std::uint32_t>(blue & 0xFF));
}

bool TestCircle(int x, int y, int cx, int cy, int radius) {
    const int dx = x - cx;
    const int dy = y - cy;
    return dx * dx + dy * dy <= radius * radius;
}
}  // namespace

/** Diagnostic-only image. This is not output from the FE7 game engine. */
extern "C" JNIEXPORT jintArray JNICALL
Java_org_fe7android_nativeport_MainActivity_nativeCreateTestFrame(
    JNIEnv* env, jobject /* thiz */) {
    constexpr int count = kTestFrameWidth * kTestFrameHeight;
    std::vector<jint> pixels(static_cast<std::size_t>(count));
    static const int swatches[][3] = {
        {220, 80, 78}, {240, 157, 73}, {245, 218, 98},
        {100, 190, 110}, {74, 166, 194}, {100, 115, 220},
        {166, 103, 204}, {237, 237, 228}, {55, 63, 78}, {18, 23, 32}
    };

    for (int y = 0; y < kTestFrameHeight; ++y) {
        for (int x = 0; x < kTestFrameWidth; ++x) {
            jint color = TestArgb(55 + y / 3, 115 + y / 4, 180 + y / 8);
            if ((TestCircle(x, y, 42, 28, 10) ||
                 TestCircle(x, y, 53, 25, 12) ||
                 TestCircle(x, y, 64, 29, 9)) && y < 45) {
                color = TestArgb(239, 244, 238);
            }
            if (TestCircle(x, y, 195, 29, 13)) color = TestArgb(255, 218, 111);

            // Peaks rise toward their centers; the old formula inverted them
            // and painted almost the entire sky as a mountain.
            const int ridgeLeft = 55 + std::abs(x - 62) * 3 / 4;
            const int ridgeRight = 68 + std::abs(x - 160) * 2 / 3;
            if (x >= 2 && x <= 151 && y >= ridgeLeft && y < 112) {
                color = TestArgb(75, 112, 126);
            }
            if (x >= 70 && x <= 238 && y >= ridgeRight && y < 112) {
                color = TestArgb(88, 124, 135);
            }
            if (y >= 111) color = TestArgb(60, 132, 75);
            if (y >= 127 && (x + y) % 17 < 3) color = TestArgb(105, 171, 83);

            if (x < 2 || x >= kTestFrameWidth - 2 || y < 2 || y >= kTestFrameHeight - 2) {
                color = TestArgb(255, 217, 138);
            }
            if (y >= 145 && y < 158 && x >= 10 && x < 230) {
                const int index = std::min((x - 10) / 22, 9);
                color = TestArgb(swatches[index][0], swatches[index][1], swatches[index][2]);
            }
            pixels[static_cast<std::size_t>(y * kTestFrameWidth + x)] = color;
        }
    }

    jintArray result = env->NewIntArray(count);
    if (result == nullptr) return nullptr;
    env->SetIntArrayRegion(result, 0, count, pixels.data());
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return nullptr;
    }
    return result;
}
