cmake_minimum_required(VERSION 3.22.1)
project(fe7native LANGUAGES C CXX)

set(MGBA_CORE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/${ANDROID_ABI}/mgba_libretro.so")
if(NOT EXISTS "${MGBA_CORE_PATH}")
    message(FATAL_ERROR "Missing mGBA core: ${MGBA_CORE_PATH}. Run the pinned mGBA build step before Gradle.")
endif()

# Optional path to the user's own extracted FE7 decompilation. The source tree
# is deliberately not copied into this repository and no ROM/assets are bundled.
set(FE7_SOURCE_ROOT "" CACHE PATH "Path to extracted fe7_us-main source tree")

add_library(mgba_libretro SHARED IMPORTED)
set_target_properties(mgba_libretro PROPERTIES IMPORTED_LOCATION "${MGBA_CORE_PATH}")

add_library(fe7native SHARED fe7native.cpp gba_bios_compat.cpp gba_memory.cpp gba_port.cpp gba_bios_services.cpp gba_dma.cpp gba_irq.cpp fe7_engine_startup.cpp)
target_compile_features(fe7native PRIVATE cxx_std_17)
target_compile_options(fe7native PRIVATE -Wall -Wextra)
target_include_directories(fe7native PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(fe7native mgba_libretro)

if(FE7_SOURCE_ROOT)
    get_filename_component(FE7_SOURCE_ROOT "${FE7_SOURCE_ROOT}" REALPATH)
    if(NOT EXISTS "${FE7_SOURCE_ROOT}/include/prelude.h" OR
       NOT IS_DIRECTORY "${FE7_SOURCE_ROOT}/src")
        message(FATAL_ERROR "FE7_SOURCE_ROOT must point to the extracted fe7_us-main source root (containing include/ and src/).")
    endif()

    # This first real-source slice consists of FE7 translation units confirmed
    # to be data-only (global game-state tables), not a pretend full engine.
    # The entrypoint, renderer, game loop and ARM7 service dependencies are
    # separate follow-up milestones.
    set(FE7_NATIVE_DATA_SOURCES
        "${FE7_SOURCE_ROOT}/src/util.c"
        "${FE7_SOURCE_ROOT}/src/text.c"
        "${FE7_SOURCE_ROOT}/src/face.c"
        "${FE7_SOURCE_ROOT}/src/hardware.c"
        "${FE7_SOURCE_ROOT}/src/proc.c"
        "${FE7_SOURCE_ROOT}/src/unit.c"
        "${FE7_SOURCE_ROOT}/src/bm.c"
        "${FE7_SOURCE_ROOT}/src/map.c"
        "${FE7_SOURCE_ROOT}/src/sprite.c"
        "${FE7_SOURCE_ROOT}/src/icon.c"
        "${FE7_SOURCE_ROOT}/src/msg.c"
        "${FE7_SOURCE_ROOT}/src/debug_text.c"
        "${FE7_SOURCE_ROOT}/src/banim_sprite.c"
    )
    foreach(FE7_SOURCE IN LISTS FE7_NATIVE_DATA_SOURCES)
        if(NOT EXISTS "${FE7_SOURCE}")
            message(FATAL_ERROR "Required FE7 data-slice source is missing: ${FE7_SOURCE}")
        endif()
    endforeach()

    add_library(fe7_engine_data OBJECT ${FE7_NATIVE_DATA_SOURCES})
    set_target_properties(fe7_engine_data PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS YES
        POSITION_INDEPENDENT_CODE YES
    )
    target_compile_definitions(fe7_engine_data PRIVATE FE7_NATIVE_PORT=1)
    target_compile_options(fe7_engine_data PRIVATE -Wall -Wextra -fno-common)
    target_include_directories(fe7_engine_data PRIVATE
        "${FE7_SOURCE_ROOT}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_sources(fe7native PRIVATE $<TARGET_OBJECTS:fe7_engine_data>)
    target_compile_definitions(fe7native PRIVATE FE7_ENGINE_SOURCE_DATA_LINKED=1)

    # Compile the adapted AgbMain entrypoint as a standalone object. It is not
    # linked into fe7native yet because its runtime/graphics/audio dependencies
    # are not fully ported; keeping the object out of the link avoids claiming
    # an engine boot when only source compatibility has been proven.
    add_library(fe7_engine_startup_probe OBJECT "${FE7_SOURCE_ROOT}/src/main.c")
    set_target_properties(fe7_engine_startup_probe PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS YES
        POSITION_INDEPENDENT_CODE YES
    )
    target_compile_definitions(fe7_engine_startup_probe PRIVATE FE7_NATIVE_PORT=1)
    target_compile_options(fe7_engine_startup_probe PRIVATE -Wall -Wextra)
    target_include_directories(fe7_engine_startup_probe PRIVATE
        "${FE7_SOURCE_ROOT}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    add_dependencies(fe7native fe7_engine_startup_probe)
    message(STATUS "FE7 native source slice enabled: 13 linked data units plus AgbMain compile probe; game boot is not yet implemented.")
else()
    message(STATUS "FE7_SOURCE_ROOT not set: building the current Android shell/runtime only.")
endif()
