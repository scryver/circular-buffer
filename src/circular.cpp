
#ifndef LINUX_BUILD
#define LINUX_BUILD 0
#endif

#ifndef WINDOWS_BUILD
#define WINDOWS_BUILD 0
#endif

#ifndef APPLE_BUILD
#define APPLE_BUILD 0
#endif

#if !LINUX_BUILD && !WINDOWS_BUILD && !APPLE_BUILD
#error Specify a platform to build for
#endif

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

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#define internal    static
#define global      static

typedef uint8_t     u8;
typedef uint16_t    u16;
typedef uint32_t    u32;
typedef uint64_t    u64;

typedef int8_t      s8;
typedef int16_t     s16;
typedef int32_t     s32;
typedef int64_t     s64;

typedef int8_t      b8;
typedef int16_t     b16;
typedef int32_t     b32;
typedef int64_t     b64;

struct CircularBuffer
{
    void *base;
    u64 byteCount;
    u64 readIndex;
    u64 writeIndex;
    void *platform;
};

#define is_pow2(x)       (!((x) & ((x) - 1)))
#define is_64k_mult(x)   (((x) & 0xFFFF) == 0)

internal void allocate_circular_buffer(CircularBuffer *buffer, u64 size);
internal void deallocate_circular_buffer(CircularBuffer *buffer);

#if APPLE_BUILD
// TODO(michiel): Compile assert 64k is multiple of vm_page_size
// NOTE(michiel): https://www.mikeash.com/pyblog/friday-qa-2012-02-03-ring-buffers-and-mirrored-memory-part-i.html

internal void
deallocate_circular_buffer(CircularBuffer *buffer)
{
    kern_return_t status = vm_deallocate(mach_task_self(), (vm_address_t)buffer->base, buffer->byteCount << 1);
    if (status == KERN_SUCCESS)
    {
        buffer->base = 0;
        buffer->byteCount = 0;
    }
    else
    {
        // TODO(michiel): Can we get a error message?
        fprintf(stderr, "Error deallocating the circular buffer.\n");
    }
}

internal void
allocate_circular_buffer(CircularBuffer *buffer, u64 size)
{
    buffer->base = 0;
    buffer->byteCount = 0;
    buffer->readIndex = 0;
    buffer->writeIndex = 0;
    if (is_64k_mult(size))
    {
        u32 numTries = 10;
        while (!buffer->base && (numTries-- != 0))
        {
            kern_return_t status = vm_allocate(mach_task_self(), (vm_address_t *)&buffer->base, size << 1, VM_FLAGS_ANYWHERE);
            if (status == KERN_SUCCESS)
            {
                u8 *target = (u8 *)buffer->base + size;
                status = vm_deallocate(mach_task_self(), (vm_address_t)target, size);
                if (status == KERN_SUCCESS)
                {
                    vm_prot_t curProtection;
                    vm_prot_t maxProtection;
                    status = vm_remap(mach_task_self(), (vm_address_t *)&target, size, 0, 0,
                                      mach_task_self(), (vm_address_t)buffer->base, 0, &curProtection, &maxProtection, VM_INHERIT_COPY);
                    if (status == KERN_SUCCESS)
                    {
                        buffer->byteCount = size;
                    }
                    else if (status == KERN_NO_SPACE)
                    {
                        if (vm_deallocate(mach_task_self(), (vm_address_t)buffer->base, size << 1) != KERN_SUCCESS) {
                            numTries = 0;
                        }
                        buffer->base = 0;
                    }
                    else
                    {
                        vm_deallocate(mach_task_self(), (vm_address_t)buffer->base, size << 1);
                        numTries = 0;
                        buffer->base = 0;
                    }
                }
                else
                {
                    vm_deallocate(mach_task_self(), (vm_address_t)buffer->base, size << 1);
                    numTries = 0;
                    buffer->base = 0;
                }
            }
            else
            {
                numTries = 0;
                buffer->base = 0;
            }
        }
    }
    else
    {
        fprintf(stderr, "The size of the circular buffer must be a multiple of 64kB.\n");
    }
}

#endif

#if WINDOWS_BUILD
// TODO(michiel): Compile assert sizeof(HANDLE) <= sizeof(void *)

// NOTE(michiel): https://fgiesen.wordpress.com/2012/07/21/the-magic-ring-buffer/
// NOTE(michiel): https://gist.github.com/rygorous/3158316

internal void *
determine_viable_address(u64 size)
{
    void *base = VirtualAlloc(0, size, MEM_RESERVE, PAGE_NOACCESS);
    if (base) {
        VirtualFree(base, 0, MEM_RELEASE);
    }
    return base;
}

internal void
deallocate_circular_buffer(CircularBuffer *buffer)
{
    if (buffer->base)
    {
        UnmapViewOfFile(buffer->base);
        UnmapViewOfFile((u8 *)buffer->base + buffer->byteCount);
        buffer->base = 0;
    }
    
    if (buffer->platform)
    {
        HANDLE handle = (HANDLE)(u64)buffer->platform;
        CloseHandle(handle);
        buffer->platform = 0;
    }
    
    buffer->byteCount = 0;
}

internal void
allocate_circular_buffer(CircularBuffer *buffer, u64 size)
{
    buffer->base = 0;
    buffer->byteCount = 0;
    buffer->readIndex = 0;
    buffer->writeIndex = 0;
    if (is_64k_mult(size))
    {
        u32 numTries = 10;
        while (!buffer->base && (numTries-- != 0))
        {
            void *target = determine_viable_address(size * 2);
            if (target) {
                u64 allocSize = size * 2;
                HANDLE mapping = CreateFileMappingA(INVALID_HANDLE_VALUE, 0, PAGE_READWRITE, (u32)(allocSize >> 32), (u32)(allocSize & 0xFFFFFFFF), 0);
                if (mapping)
                {
                    buffer->platform = (void *)(u64)mapping;
                    buffer->base = MapViewOfFileEx(mapping, FILE_MAP_ALL_ACCESS, 0, 0, size, target);
                    if (buffer->base)
                    {
                        if (MapViewOfFileEx(mapping, FILE_MAP_ALL_ACCESS, 0, 0, size, (u8 *)target + size))
                        {
                            buffer->byteCount = size;
                        }
                        else
                        {
                            deallocate_circular_buffer(buffer);
                        }
                    }
                    else
                    {
                        deallocate_circular_buffer(buffer);
                    }
                }
            }
        }
    }
    else
    {
        fprintf(stderr, "The size of the circular buffer must be a multiple of 64kB.\n");
    }
}
#endif

#if LINUX_BUILD

internal void
deallocate_circular_buffer(CircularBuffer *buffer)
{
    s32 status = munmap(buffer->base, buffer->byteCount << 1);
    if (status == 0) 
    {
        buffer->base = 0;
        buffer->byteCount = 0;
    }
    else
    {
        fprintf(stderr, "Error unmapping the circular buffer: %s\n", strerror(errno));
    }
}

internal void
allocate_circular_buffer(CircularBuffer *buffer, u64 size)
{
    buffer->base = 0;
    buffer->byteCount = 0;
    buffer->readIndex = 0;
    buffer->writeIndex = 0;
    if (is_64k_mult(size))
    {
        char tempPath[] = "/dev/shm/circ-buf-XXXXXX";
        s32 fd = mkstemp(tempPath);
        if (fd >= 0)
        {
            s32 status = unlink(tempPath);
            if (status == 0)
            {
                status = ftruncate(fd, size);
                if (status == 0)
                {
                    buffer->base = mmap(0, size << 1, 
                                        PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
                    if (buffer->base != MAP_FAILED)
                    {
                        void *map1 = mmap(buffer->base, size, 
                                          PROT_READ | PROT_WRITE, MAP_FIXED | MAP_SHARED, fd, 0);
                        if (map1 == buffer->base)
                        {
                            void *map2 = mmap((u8 *)buffer->base + size, size, 
                                              PROT_READ | PROT_WRITE, MAP_FIXED | MAP_SHARED, fd, 0);
                            if (map2 == ((u8 *)buffer->base + size))
                            {
                                buffer->byteCount = size;
                            }
                            else
                            {
                                deallocate_circular_buffer(buffer);
                                fprintf(stderr, "Couldn't map the second part of the circular buffer.\n");
                            }
                        }
                        else
                        {
                            deallocate_circular_buffer(buffer);
                            fprintf(stderr, "Couldn't map the first part of the circular buffer.\n");
                        }
                    }
                    else
                    {
                        buffer->base = 0;
                        fprintf(stderr, "Couldn't map the amount of memory needed.\n");
                    }
                }
                else
                {
                    fprintf(stderr, "Couldn't truncate the temp file to the appropiate byte size: %s\n", strerror(errno));
                }
            }
            else
            {
                fprintf(stderr, "Couldn't unlink the temp path: %s\n", strerror(errno));
            }
            
            status = close(fd);
            if (status)
            {
                fprintf(stderr, "Error happened while closing the temp file: %s\n", strerror(errno));
            }
        }
        else
        {
            fprintf(stderr, "Couldn't make a temp path: %s\n", strerror(errno));
        }
    }
    else
    {
        fprintf(stderr, "The size of the circular buffer must be a multiple of 64kB.\n");
    }
}

#endif // LINUX_BUILD

internal void
clear(CircularBuffer *buffer)
{
    buffer->readIndex = 0;
    buffer->writeIndex = 0;
}

internal u64
get_size(CircularBuffer *buffer)
{
    u64 result = buffer->writeIndex - buffer->readIndex;
    return result;
}

internal u64
get_available_size(CircularBuffer *buffer)
{
    return buffer->byteCount - get_size(buffer);
}

internal void *
get_write_pointer(CircularBuffer *buffer)
{
    return (u8 *)buffer->base + buffer->writeIndex;
}

internal void
write_advance(CircularBuffer *buffer, u64 byteCount)
{
    // TODO(michiel): Maybe add some debug checking here (writeIndex >= readIndex)
    // TODO(michiel): This should be atomic to be thread-safe
    buffer->writeIndex += byteCount;
}

internal void *
get_read_pointer(CircularBuffer *buffer)
{
    return (u8 *)buffer->base + buffer->readIndex;
}

internal void
read_advance(CircularBuffer *buffer, u64 byteCount)
{
    // TODO(michiel): Maybe add some debug checking here (readIndex + byteCount <= writeIndex)
    buffer->readIndex += byteCount;
    
    if (buffer->readIndex >= buffer->byteCount)
    {
        buffer->readIndex -= buffer->byteCount;
        // TODO(michiel): This should be atomic to be thread-safe
        buffer->writeIndex -= buffer->byteCount;
    }
}

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
