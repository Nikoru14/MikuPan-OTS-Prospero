#ifndef MIKUPAN_MIKUPAN_PS5_H
#define MIKUPAN_MIKUPAN_PS5_H

#ifdef __PROSPERO__

#ifdef __cplusplus
extern "C" {
#endif

/// The writable folder for settings, the baked IMG_TEX.BIN, saves and the
/// log, with a trailing separator: /data/MikuPan/ when the title can write
/// there, like other homebrew, otherwise the title's own /download0/MikuPan/.
/// Never returns NULL. Implemented in mikupan_file.cpp.
const char *MikuPan_GetPs5UserDirectory(void);

/// Whether a file or folder exists, found by opening it: on the console,
/// libc++'s std::filesystem gets EINVAL from stat. Implemented in
/// mikupan_file.cpp.
bool MikuPan_Ps5PathExists(const char *path);

/// Shows a PS5 system notification (the pop-up in the top corner), and
/// writes the same text to the log. Useful while the game has no other way
/// to show anything on the console.
void MikuPan_Ps5Notify(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/// Sends SDL's own log messages (SDL_GPU's reasons for refusing a driver
/// among them) to MikuPan's log, at every priority for SDL's GPU, video and
/// system categories. Call before SDL_Init.
void MikuPan_Ps5ForwardSdlLog(void);

/// Walks through what SDL_GPU's Vulkan backend needs from RADV (an
/// instance, a GPU, its features and extensions, a graphics queue, the
/// display) and logs each answer, to say why SDL_GPU refused Vulkan.
void MikuPan_Ps5VulkanReport(void);

#ifdef __cplusplus
}
#endif

#endif // __PROSPERO__

#endif // MIKUPAN_MIKUPAN_PS5_H
