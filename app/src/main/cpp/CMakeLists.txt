cmake_minimum_required(VERSION 3.22.1)
project(fe7native LANGUAGES C CXX ASM)

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

    # This first real-source slice consists of low-dependency FE7 C translation
    # units (some contain helper logic and some declare global game state), not
    # a pretend full engine.
    # The entrypoint, renderer, game loop and ARM7 service dependencies are
    # separate follow-up milestones.
    set(FE7_NATIVE_SEED_SOURCES
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
    foreach(FE7_SOURCE IN LISTS FE7_NATIVE_SEED_SOURCES)
        if(NOT EXISTS "${FE7_SOURCE}")
            message(FATAL_ERROR "Required FE7 data-slice source is missing: ${FE7_SOURCE}")
        endif()
    endforeach()

    add_library(fe7_engine_seed OBJECT ${FE7_NATIVE_SEED_SOURCES})
    set_target_properties(fe7_engine_seed PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS YES
        POSITION_INDEPENDENT_CODE YES
    )
    target_compile_definitions(fe7_engine_seed PRIVATE FE7_NATIVE_PORT=1)
    target_compile_options(fe7_engine_seed PRIVATE -Wall -Wextra -fno-common)
    target_include_directories(fe7_engine_seed PRIVATE
        "${FE7_SOURCE_ROOT}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_sources(fe7native PRIVATE $<TARGET_OBJECTS:fe7_engine_seed>)
    target_compile_definitions(fe7native PRIVATE FE7_ENGINE_SOURCE_SEED_LINKED=1)

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

    # Additional real C engine/service modules are compiled for every Android
    # ABI, but held out of the shared-library link until their dependencies are
    # mapped or replaced. This turns incompatibilities into build-time feedback
    # without shipping a library with unresolved engine symbols.
    set(FE7_NATIVE_SERVICE_PROBE_SOURCES
        "${FE7_SOURCE_ROOT}/src/async_upload.c"
        "${FE7_SOURCE_ROOT}/src/audio.c"
        "${FE7_SOURCE_ROOT}/src/event.c"
        "${FE7_SOURCE_ROOT}/src/gbasram.c"
        "${FE7_SOURCE_ROOT}/src/interrupt.c"
    )
    foreach(FE7_SERVICE_SOURCE IN LISTS FE7_NATIVE_SERVICE_PROBE_SOURCES)
        if(NOT EXISTS "${FE7_SERVICE_SOURCE}")
            message(FATAL_ERROR "Required FE7 service source is missing: ${FE7_SERVICE_SOURCE}")
        endif()
    endforeach()
    add_library(fe7_engine_service_probe OBJECT ${FE7_NATIVE_SERVICE_PROBE_SOURCES})
    set_target_properties(fe7_engine_service_probe PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS YES
        POSITION_INDEPENDENT_CODE YES
    )
    target_compile_definitions(fe7_engine_service_probe PRIVATE FE7_NATIVE_PORT=1)
    target_compile_options(fe7_engine_service_probe PRIVATE -Wall -Wextra)
    target_include_directories(fe7_engine_service_probe PRIVATE
        "${FE7_SOURCE_ROOT}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    add_dependencies(fe7native fe7_engine_service_probe)

    # The original assembly is buildable as ARMv7 objects for the 32-bit
    # Android ABI. Keep this as a compile probe for now: the GBA absolute
    # memory layout and ROM-backed data sections prevent linking/execution yet.
    if(ANDROID_ABI STREQUAL "armeabi-v7a")
        # The C sound driver compiles for ARMv7 but contains a GBA SWI call,
        # so it is a compile-only probe and is not linked/executed.
        if(NOT EXISTS "${FE7_SOURCE_ROOT}/src/m4a.c")
            message(FATAL_ERROR "Required FE7 sound driver source is missing: ${FE7_SOURCE_ROOT}/src/m4a.c")
        endif()
        add_library(fe7_engine_m4a_probe OBJECT "${FE7_SOURCE_ROOT}/src/m4a.c")
        set_target_properties(fe7_engine_m4a_probe PROPERTIES
            C_STANDARD 11
            C_STANDARD_REQUIRED YES
            C_EXTENSIONS YES
            POSITION_INDEPENDENT_CODE YES
        )
        target_compile_definitions(fe7_engine_m4a_probe PRIVATE FE7_NATIVE_PORT=1)
        target_compile_options(fe7_engine_m4a_probe PRIVATE -Wall -Wextra)
        target_include_directories(fe7_engine_m4a_probe PRIVATE
            "${FE7_SOURCE_ROOT}/include"
            "${CMAKE_CURRENT_SOURCE_DIR}"
        )
        add_dependencies(fe7native fe7_engine_m4a_probe)

        find_package(Python3 COMPONENTS Interpreter REQUIRED)
        get_filename_component(FE7_NATIVE_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../" ABSOLUTE)
        set(FE7_ASM_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/fe7-asm")
        set(FE7_ASM_NORMALIZED_CODE "${FE7_ASM_BUILD_DIR}/code-android.s")
        add_custom_command(
            OUTPUT "${FE7_ASM_NORMALIZED_CODE}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${FE7_ASM_BUILD_DIR}"
            COMMAND "${Python3_EXECUTABLE}"
                    "${FE7_NATIVE_REPO_ROOT}/scripts/prepare_fe7_native_assembly.py"
                    "${FE7_SOURCE_ROOT}/asm/code.s"
                    "${FE7_ASM_NORMALIZED_CODE}"
            DEPENDS
                "${FE7_SOURCE_ROOT}/asm/code.s"
                "${FE7_NATIVE_REPO_ROOT}/scripts/prepare_fe7_native_assembly.py"
            VERBATIM
        )
        set_source_files_properties("${FE7_ASM_NORMALIZED_CODE}" PROPERTIES GENERATED TRUE)
        set(FE7_NATIVE_ASM_SOURCES
            "${FE7_ASM_NORMALIZED_CODE}"
            "${FE7_SOURCE_ROOT}/asm/code_1.s"
            "${FE7_SOURCE_ROOT}/asm/event.s"
            "${FE7_SOURCE_ROOT}/asm/fake_glue.s"
            "${FE7_SOURCE_ROOT}/src/armfunc.s"
            "${FE7_SOURCE_ROOT}/src/crt0.s"
            "${FE7_SOURCE_ROOT}/src/gbasvc.s"
            "${FE7_SOURCE_ROOT}/src/sio_link_loader.s"
        )
        foreach(FE7_ASM_SOURCE IN LISTS FE7_NATIVE_ASM_SOURCES)
            if("${FE7_ASM_SOURCE}" STREQUAL "${FE7_ASM_NORMALIZED_CODE}")
                # Generated by prepare_fe7_native_assembly.py at build time.
            elseif(NOT EXISTS "${FE7_ASM_SOURCE}")
                message(FATAL_ERROR "Required FE7 assembly source is missing: ${FE7_ASM_SOURCE}")
            endif()
        endforeach()
        add_library(fe7_engine_asm_probe OBJECT ${FE7_NATIVE_ASM_SOURCES})
        target_include_directories(fe7_engine_asm_probe PRIVATE
            "${FE7_SOURCE_ROOT}"
            "${FE7_SOURCE_ROOT}/include"
        )
        set_target_properties(fe7_engine_asm_probe PROPERTIES POSITION_INDEPENDENT_CODE YES)
        add_dependencies(fe7native fe7_engine_asm_probe)
        message(STATUS "FE7 ARMv7 sound/assembly probes enabled: m4a.c plus 8 original ASM units compile only; they are not linked or executed.")
    endif()

    message(STATUS "FE7 native source slice enabled: 13 linked C units plus separate engine/startup compile probes; game boot is not yet implemented.")
else()
    message(STATUS "FE7_SOURCE_ROOT not set: building the current Android shell/runtime only.")
endif()
