/*
 * MikuPan PS5 title: helpers the ps5-payload-sdk's libc expects from a payload
 * loader.
 *
 * The SDK's libc wraps mmap/mprotect and calls kernel_mprotect when it is
 * present. A payload's ELF loader provides that helper; a native title has no
 * such loader, and the title converter rejects any import, even a weak one,
 * that no system module exports. Provide it here for the calling process,
 * forwarding to the kernel's own sceKernelMprotect.
 */

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

extern "C" int sceKernelMprotect(const void *address, size_t length, int protection);

extern "C" int kernel_mprotect(pid_t pid, intptr_t address, size_t length,
                               int protection)
{
    (void)pid; /* Only the title's own process is ever changed. */
    return sceKernelMprotect(reinterpret_cast<const void *>(address), length,
                             protection) == 0
               ? 0
               : -1;
}
