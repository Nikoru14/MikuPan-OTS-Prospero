# PlayStation 5 (Prospero) homebrew build, using the community
# ps5-payload-sdk toolchain:
#
#   cmake -B build-ps5 -G Ninja \
#         -DCMAKE_TOOLCHAIN_FILE=$PS5_PAYLOAD_SDK/toolchain/prospero.cmake \
#         -DCMAKE_BUILD_TYPE=Debug -DMIKUPAN_COMPILE_SHADERS=OFF
#
# The toolchain targets x86_64-sie-ps5, which reports itself as FreeBSD, so
# SDL3 would otherwise look for desktop FreeBSD backends (X11, Wayland, OSS,
# udev, D-Bus...). None of those exist on the console: switch them all off and
# keep only what a console build can use. Video and audio start as SDL's dummy
# drivers until the PS5 backends are written; Vulkan and SDL_GPU stay on for
# the community Mesa/RADV driver.

if(NOT PS5)
    return()
endif()

message(STATUS "MikuPan: configuring for PlayStation 5 (ps5-payload-sdk)")

function(mikupan_ps5_force name value)
    set(${name} ${value} CACHE BOOL "Forced by MikuPan's PS5 build" FORCE)
endfunction()

# --- SDL3 --------------------------------------------------------------------
foreach(sdl_off
        SDL_SHARED SDL_TESTS SDL_TEST_LIBRARY SDL_EXAMPLES SDL_INSTALL
        SDL_X11 SDL_WAYLAND SDL_KMSDRM SDL_RPI SDL_VIVANTE SDL_OFFSCREEN_EGL
        SDL_OPENGL SDL_OPENGLES
        SDL_ALSA SDL_JACK SDL_PIPEWIRE SDL_PULSEAUDIO SDL_SNDIO SDL_OSS SDL_DISKAUDIO
        SDL_DBUS SDL_IBUS SDL_LIBUDEV SDL_HIDAPI SDL_HIDAPI_LIBUSB SDL_LIBURING
        SDL_CAMERA)
    mikupan_ps5_force(${sdl_off} OFF)
endforeach()

# SDL_UNIX_CONSOLE_BUILD skips SDL's "no X11 or Wayland" desktop check, which
# is the documented switch for Unix-like targets without desktop windows.
foreach(sdl_on
        SDL_STATIC SDL_VULKAN SDL_GPU SDL_RENDER_GPU
        SDL_DUMMYVIDEO SDL_OFFSCREEN SDL_DUMMYAUDIO SDL_UNIX_CONSOLE_BUILD)
    mikupan_ps5_force(${sdl_on} ON)
endforeach()

# --- Title packaging ---------------------------------------------------------
# The game is linked as a native title (not a payload) and staged as a folder
# that ShadowMount+ can mount:
#
#   build-ps5/app/<TITLE_ID>/
#     eboot.bin              converted and fake-signed by ps5-native-tool
#     sce_sys/param.json     from ps5/sce_sys/param.json.in
#     sce_sys/icon0.png
#     sce_module/libc.prx    clean-room runtime module
#     resources/             the game's own resources
#
# ps5-native-tool and libc.prx come from ps5-native-app-boilerplate, built on
# the host (see the README). Without them only the ELF is built.
set(MIKUPAN_PS5_DIR "${CMAKE_SOURCE_DIR}/ps5")
set(MIKUPAN_PS5_TITLE_ID "PPSA39390" CACHE STRING
        "PS5 title ID: PPSA followed by five digits, unique on the console")
set(MIKUPAN_PS5_CONCEPT_ID "39390" CACHE STRING "PS5 concept ID: five digits")
set(MIKUPAN_PS5_CONTENT_SUFFIX "MIKUPANPROSPERO1" CACHE STRING
        "Last part of the PS5 content ID: 16 characters A-Z or 0-9")
set(MIKUPAN_PS5_TITLE_NAME "MikuPan OTS" CACHE STRING "Name shown on the PS5 home screen")
set(MIKUPAN_PS5_NATIVE_TOOL "" CACHE FILEPATH
        "ps5-native-tool from ps5-native-app-boilerplate")
set(MIKUPAN_PS5_RUNTIME_MODULE "" CACHE FILEPATH
        "libc.prx from ps5-native-app-boilerplate")

if(NOT MIKUPAN_PS5_TITLE_ID MATCHES "^PPSA[0-9][0-9][0-9][0-9][0-9]$")
    message(FATAL_ERROR "MIKUPAN_PS5_TITLE_ID must be PPSA followed by five digits")
endif()
if(NOT MIKUPAN_PS5_CONCEPT_ID MATCHES "^[0-9][0-9][0-9][0-9][0-9]$")
    message(FATAL_ERROR "MIKUPAN_PS5_CONCEPT_ID must be five digits")
endif()
if(NOT MIKUPAN_PS5_CONTENT_SUFFIX MATCHES "^[A-Z0-9][A-Z0-9][A-Z0-9][A-Z0-9][A-Z0-9][A-Z0-9][A-Z0-9][A-Z0-9][A-Z0-9][A-Z0-9][A-Z0-9][A-Z0-9][A-Z0-9][A-Z0-9][A-Z0-9][A-Z0-9]$")
    message(FATAL_ERROR "MIKUPAN_PS5_CONTENT_SUFFIX must be 16 characters A-Z or 0-9")
endif()
set(MIKUPAN_PS5_CONTENT_ID
        "UP9000-${MIKUPAN_PS5_TITLE_ID}_00-${MIKUPAN_PS5_CONTENT_SUFFIX}")

# Loader and container constants used by ps5-native-app-boilerplate, which it
# validated on firmware 6.02 and 12.70.
set(MIKUPAN_PS5_MODULE_SDK 0x02000009)
set(MIKUPAN_PS5_COMPANION_SDK 0x08050001)
set(MIKUPAN_PS5_FSELF_MAGIC 0x1D3D154F)

function(mikupan_ps5_title target)
    set(native "${MIKUPAN_PS5_DIR}/native")

    target_sources(${target} PRIVATE
            "${native}/app_crt.cpp"
            "${native}/cxa_thread_atexit.cpp"
            "${native}/payload_shims.cpp")

    # The SDK's compiler wrapper links the WebKit-only libkernel_web unless
    # libkernel is requested; a title binds against libkernel. The console
    # has no general dlopen service, so SDL's dynamic loading resolves to
    # nothing, as in the OpenLF2 PS5 port.
    target_link_libraries(${target} PRIVATE kernel)
    target_link_options(${target} PRIVATE
            -nostartfiles
            "LINKER:-e,_start"
            "LINKER:--eh-frame-hdr"
            "LINKER:--defsym=__dlopen=0"
            "LINKER:--defsym=__dlclose=0"
            "LINKER:--defsym=__dlsym=0"
            "LINKER:--defsym=__dladdr=0"
            "LINKER:--defsym=__dlerror=0"
            "LINKER:--version-script=${native}/app-symbols.map"
            "LINKER:-T,${native}/ps5-pie.ld")
    set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS
            "${native}/app-symbols.map" "${native}/ps5-pie.ld")

    if(NOT MIKUPAN_PS5_NATIVE_TOOL OR NOT MIKUPAN_PS5_RUNTIME_MODULE)
        message(STATUS "MikuPan: PS5 title folder skipped; set "
                "MIKUPAN_PS5_NATIVE_TOOL and MIKUPAN_PS5_RUNTIME_MODULE to build it")
        return()
    endif()
    foreach(required MIKUPAN_PS5_NATIVE_TOOL MIKUPAN_PS5_RUNTIME_MODULE)
        if(NOT EXISTS "${${required}}")
            message(FATAL_ERROR "${required} does not exist: ${${required}}")
        endif()
    endforeach()

    set(app "${CMAKE_BINARY_DIR}/app/${MIKUPAN_PS5_TITLE_ID}")
    set(eboot_elf "${CMAKE_BINARY_DIR}/ps5/eboot.elf")
    set(param "${CMAKE_BINARY_DIR}/ps5/param.json")
    configure_file("${MIKUPAN_PS5_DIR}/sce_sys/param.json.in" "${param}" @ONLY)

    set(tool "${MIKUPAN_PS5_NATIVE_TOOL}")
    add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                    "${CMAKE_BINARY_DIR}/ps5" "${app}/sce_sys" "${app}/sce_module"
            COMMAND "${tool}" link --in "$<TARGET_FILE:${target}>" --out "${eboot_elf}"
                    --stub-dir "${PS5_PAYLOAD_SDK}/target/lib"
                    --module-sdk ${MIKUPAN_PS5_MODULE_SDK}
                    --companion-sdk ${MIKUPAN_PS5_COMPANION_SDK}
                    --file-name eboot.elf
            COMMAND "${tool}" self --sign --in "${eboot_elf}" --out "${app}/eboot.bin"
                    --magic ${MIKUPAN_PS5_FSELF_MAGIC}
            COMMAND "${tool}" self --inspect --file "${app}/eboot.bin"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${param}"
                    "${app}/sce_sys/param.json"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${MIKUPAN_PS5_DIR}/sce_sys/icon0.png" "${app}/sce_sys/icon0.png"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${MIKUPAN_PS5_RUNTIME_MODULE}" "${app}/sce_module/libc.prx"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${CMAKE_SOURCE_DIR}/resources" "${app}/resources"
            COMMENT "Staging PS5 title folder ${app}"
            VERBATIM)
    message(STATUS "MikuPan: PS5 title folder ${app}")
endfunction()

# --- FFmpeg ------------------------------------------------------------------
# Passed to FFmpeg's own configure script by cmake/ffmpeg.cmake.
set(MIKUPAN_FFMPEG_CROSS_ARGS
        --enable-cross-compile
        --target-os=freebsd
        --arch=x86_64
        --ar=${CMAKE_AR}
        --ranlib=${CMAKE_RANLIB}
        --nm=${PS5_PAYLOAD_SDK}/bin/prospero-nm
        --strip=${CMAKE_STRIP}
        --host-cc=cc
        --pkg-config=false)
