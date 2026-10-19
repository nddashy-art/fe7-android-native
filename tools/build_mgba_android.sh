#!/usr/bin/env bash
set -euo pipefail

# Pin mGBA so CI builds are reproducible. No ROM or BIOS is downloaded.
MGBA_COMMIT="9660a668f2093cb4aba5983f52bebf8878dc5b8b"
MGBA_DIR="${RUNNER_TEMP:-/tmp}/mgba-source"

git clone --filter=blob:none https://github.com/mgba-emu/mgba.git "$MGBA_DIR"
git -C "$MGBA_DIR" checkout "$MGBA_COMMIT"

NDK="${ANDROID_NDK_HOME:-$ANDROID_HOME/ndk/27.2.12479018}"
if [[ ! -d "$NDK" ]]; then
  echo "Android NDK not found at $NDK" >&2
  exit 1
fi

for ABI in arm64-v8a armeabi-v7a x86; do
  BUILD_DIR="${RUNNER_TEMP:-/tmp}/mgba-build-$ABI"
  cmake -S "$MGBA_DIR" -B "$BUILD_DIR" \
    -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI="$ABI" \
    -DANDROID_PLATFORM=android-23 \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DBUILD_LIBRETRO=ON \
    -DBUILD_QT=OFF \
    -DBUILD_SDL=OFF \
    -DBUILD_SHARED=OFF \
    -DBUILD_STATIC=OFF \
    -DBUILD_TEST=OFF \
    -DBUILD_SUITE=OFF \
    -DBUILD_PERF=OFF \
    -DBUILD_LTO=OFF \
    -DUSE_ZLIB=OFF \
    -DUSE_PNG=OFF \
    -DUSE_SQLITE3=OFF \
    -DUSE_LUA=OFF \
    -DUSE_DISCORD_RPC=OFF \
    -DUSE_FFMPEG=OFF \
    -DUSE_LIBZIP=OFF \
    -DUSE_ELF=OFF \
    -DUSE_EDITLINE=OFF \
    -DENABLE_DEBUGGERS=OFF \
    -DENABLE_SCRIPTING=OFF \
    -DM_CORE_GBA=ON \
    -DM_CORE_GB=OFF
  cmake --build "$BUILD_DIR" --target mgba_libretro --parallel 2
  CORE="$(find "$BUILD_DIR" -type f -name 'mgba_libretro.so' -print -quit)"
  if [[ -z "$CORE" ]]; then
    echo "mGBA core output missing for $ABI" >&2
    exit 1
  fi
  mkdir -p "app/src/main/jniLibs/$ABI"
  cp "$CORE" "app/src/main/jniLibs/$ABI/mgba_libretro.so"
  echo "Built mGBA core for $ABI"
done

mkdir -p app/src/main/assets/licenses
cp "$MGBA_DIR/LICENSE" app/src/main/assets/licenses/MGBA-MPL-2.0.txt
printf '%s\n' "$MGBA_COMMIT" > app/src/main/assets/licenses/MGBA-UPSTREAM-COMMIT.txt
