cmake_minimum_required(VERSION 3.22.1)
project(fe7native LANGUAGES CXX)

set(MGBA_CORE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/${ANDROID_ABI}/mgba_libretro.so")
if(NOT EXISTS "${MGBA_CORE_PATH}")
    message(FATAL_ERROR "Missing mGBA core: ${MGBA_CORE_PATH}. Run the pinned mGBA build step before Gradle.")
endif()

add_library(mgba_libretro SHARED IMPORTED)
set_target_properties(mgba_libretro PROPERTIES IMPORTED_LOCATION "${MGBA_CORE_PATH}")

add_library(fe7native SHARED fe7native.cpp gba_bios_compat.cpp gba_memory.cpp gba_port.cpp gba_bios_services.cpp gba_dma.cpp)
target_compile_features(fe7native PRIVATE cxx_std_17)
target_compile_options(fe7native PRIVATE -Wall -Wextra)
target_include_directories(fe7native PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(fe7native mgba_libretro)
