#include <jni.h>

#include <cstdint>
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
