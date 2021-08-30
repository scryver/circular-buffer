
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "circular.h"

#if LINUX_BUILD
#include <sys/mman.h>
#endif

#if APPLE_BUILD
#include <mach/mach.h>
#include <mach/vm_map.h>
#endif

#if LINUX_BUILD || APPLE_BUILD
#include <errno.h>
#include <unistd.h>
#endif

#if WINDOWS_BUILD
#define _CRT_SECURE_NO_DEPRECATE
#include <Windows.h>
#endif

#include "circular.cpp"

s32 main(s32 argCount, char **arguments)
{
    CircularBuffer testBuf;
    allocate_circular_buffer(&testBuf, 3 * 64 * 1024);

    // NOTE(michiel): Should output:
    // R: 23992, W: 24000
    // R: 47984, W: 48000
    // R: 71976, W: 72000
    // R: 95968, W: 96000
    // R: 119960, W: 120000
    // R: 143952, W: 144000
    // R: 167944, W: 168000
    // R: 191936, W: 192000
    // R: 19320, W: 19392
    // R: 43312, W: 43392

    if (testBuf.base && testBuf.byteCount)
    {
        for (u32 testRun = 0; testRun < 10; ++testRun)
        {
            for (u32 testIndex = 0; testIndex < 8; ++testIndex)
            {
                memset(get_write_pointer(&testBuf), testIndex, 3000);
                write_advance(&testBuf, 3000);
            }
            for (u32 testIndex = 0; testIndex < 8; ++testIndex)
            {
                read_advance(&testBuf, 2999);
            }
            fprintf(stdout, "R: %lu, W: %lu\n", testBuf.readIndex, testBuf.writeIndex);
        }
    }

    return 0;
}
