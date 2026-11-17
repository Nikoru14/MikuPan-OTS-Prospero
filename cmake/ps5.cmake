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
