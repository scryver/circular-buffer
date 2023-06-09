
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define CIRCULAR_LOG(...)

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
    u64 size = 4 * 64 * 1024;
    CircularBuffer testBuf;
    allocate_circular_buffer(&testBuf, size);

    fprintf(stdout, "Size: %lu, Mask: %lu [0x%lX]\n", size, testBuf.indexMask, testBuf.indexMask);

    // NOTE(michiel): Should output:
    // R:  23992, W:  24000 [ 23992,  24000]
    // R:  47984, W:  48000 [ 47984,  48000]
    // R:  71976, W:  72000 [ 71976,  72000]
    // R:  95968, W:  96000 [ 95968,  96000]
    // R: 119960, W: 120000 [119960, 120000]
    // R: 143952, W: 144000 [143952, 144000]
    // R: 167944, W: 168000 [167944, 168000]
    // R: 191936, W: 192000 [191936, 192000]
    // R: 215928, W: 216000 [215928, 216000]
    // R: 239920, W: 240000 [239920, 240000]
    // R: 263912, W: 270000 [  1768,   7856]
    // R: 287904, W: 300000 [ 25760,  37856]
    // R: 311896, W: 330000 [ 49752,  67856]
    // R: 335888, W: 360000 [ 73744,  97856]
    // R: 359880, W: 390000 [ 97736, 127856]
    // R: 383872, W: 420000 [121728, 157856]
    // R: 407864, W: 450000 [145720, 187856]
    // R: 431856, W: 480000 [169712, 217856]
    // R: 455848, W: 510000 [193704, 247856]
    // R: 479840, W: 540000 [217696,  15712]

    if (testBuf.base && testBuf.indexMask)
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
            fprintf(stdout, "R: %6lu, W: %6lu [%6lu, %6lu]\n", testBuf.readIndex, testBuf.writeIndex, testBuf.readIndex & testBuf.indexMask, testBuf.writeIndex & testBuf.indexMask);
        }

        for (u32 testRun = 0; testRun < 10; ++testRun)
        {
            for (u32 testIndex = 0; testIndex < 10; ++testIndex)
            {
                memset(get_write_pointer(&testBuf), testIndex, 3000);
                write_advance(&testBuf, 3000);
            }
            for (u32 testIndex = 0; testIndex < 8; ++testIndex)
            {
                read_advance(&testBuf, 2999);
            }
            fprintf(stdout, "R: %6lu, W: %6lu [%6lu, %6lu]\n", testBuf.readIndex, testBuf.writeIndex, testBuf.readIndex & testBuf.indexMask, testBuf.writeIndex & testBuf.indexMask);
        }
    }

    return 0;
}
