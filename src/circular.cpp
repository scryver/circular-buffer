
// NOTE(michiel): APPLE:
//     TODO: Compile assert 64k is multiple of vm_page_size
//     Ref: https://www.mikeash.com/pyblog/friday-qa-2012-02-03-ring-buffers-and-mirrored-memory-part-i.html
// NOTE(michiel): WINDOWS:
//     TODO: Compile assert sizeof(HANDLE) <= sizeof(void *)
//     Ref: https://fgiesen.wordpress.com/2012/07/21/the-magic-ring-buffer/
//          https://gist.github.com/rygorous/3158316
// NOTE(michiel): LINUX:
//     Ref: https://en.wikipedia.org/w/index.php?title=Circular_buffer&oldid=600431497

#if APPLE_BUILD

CIRCULAR_INTERN void
deallocate_circular_buffer(CircularBuffer *buffer)
{
    u64 byteCount = buffer->indexMask + 1;
    kern_return_t status = vm_deallocate(mach_task_self(), (vm_address_t)buffer->base, byteCount << 1);
    if (status == KERN_SUCCESS)
    {
        buffer->base = 0;
        buffer->indexMask = 0;
    }
    else
    {
        CIRCULAR_ERROR("Error deallocating the circular buffer.");
    }
}

CIRCULAR_INTERN void
allocate_circular_buffer(CircularBuffer *buffer, u64 size)
{
    buffer->base = 0;
    buffer->indexMask = 0;
    buffer->readIndex = 0;
    buffer->writeIndex = 0;
    if (is_64k_mult(size) && is_pow2(size))
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
                        buffer->indexMask = size - 1;
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
        CIRCULAR_ERROR("The size of the circular buffer must be a multiple of 64kB and power of 2.");
    }
}

#elif WINDOWS_BUILD

CIRCULAR_INTERN void
deallocate_circular_buffer(CircularBuffer *buffer)
{
    if (buffer->base)
    {
        UnmapViewOfFile(buffer->base);
        UnmapViewOfFile(buffer->base + buffer->indexMask + 1);
    }

    buffer->base = 0;
    buffer->indexMask = 0;
}

CIRCULAR_INTERN void *
determine_viable_address(u64 size)
{
    void *base = VirtualAlloc(0, size, MEM_RESERVE, PAGE_NOACCESS);
    if (base) {
        VirtualFree(base, 0, MEM_RELEASE);
    }
    return base;
}

CIRCULAR_INTERN void
allocate_circular_buffer(CircularBuffer *buffer, u64 size)
{
    buffer->base = 0;
    buffer->indexMask = 0;
    buffer->readIndex = 0;
    buffer->writeIndex = 0;
    if (is_64k_mult(size) && is_pow2(size))
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
                    buffer->base = (u8 *)MapViewOfFileEx(mapping, FILE_MAP_ALL_ACCESS, 0, 0, size, target);
                    if (buffer->base)
                    {
                        if (MapViewOfFileEx(mapping, FILE_MAP_ALL_ACCESS, 0, 0, size, (u8 *)target + size))
                        {
                            buffer->indexMask = size - 1;
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

                    CloseHandle(mapping);
                }
                else
                {
                    CIRCULAR_ERROR("Couldn't create a file mapping: %d", GetLastError());
                }
            }
        }
    }
    else
    {
        CIRCULAR_ERROR("The size of the circular buffer must be a multiple of 64kB and power of 2.");
    }
}

#elif LINUX_BUILD

CIRCULAR_INTERN void
deallocate_circular_buffer(CircularBuffer *buffer)
{
    u64 byteCount = buffer->indexMask + 1;
    s32 status = munmap(buffer->base, byteCount << 1);
    if (status == 0)
    {
        buffer->base = 0;
        buffer->indexMask = 0;
    }
    else
    {
        CIRCULAR_ERROR("Error unmapping the circular buffer: %s.", strerror(errno));
    }
}

CIRCULAR_INTERN void
allocate_circular_buffer(CircularBuffer *buffer, u64 size)
{
    buffer->base = 0;
    buffer->indexMask = 0;
    buffer->readIndex = 0;
    buffer->writeIndex = 0;
    if (is_64k_mult(size) && is_pow2(size))
    {
        s32 fd = memfd_create("/circbuf", 0);
        if (fd >= 0)
        {
            s32 status = ftruncate(fd, size);
            if (status == 0)
            {
                buffer->base = (u8 *)mmap(0, size << 1, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
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
                            buffer->indexMask = size - 1;
                        }
                        else
                        {
                            deallocate_circular_buffer(buffer);
                            CIRCULAR_ERROR("Couldn't map the second part of the circular buffer.");
                        }
                    }
                    else
                    {
                        deallocate_circular_buffer(buffer);
                        CIRCULAR_ERROR("Couldn't map the first part of the circular buffer.");
                    }
                }
                else
                {
                    buffer->base = 0;
                    CIRCULAR_ERROR("Couldn't map the amount of memory needed.");
                }
            }
            else
            {
                CIRCULAR_ERROR("Couldn't truncate the temp file to the appropiate byte size: %s", strerror(errno));
            }

            status = close(fd);
            if (status)
            {
                CIRCULAR_ERROR("Error happened while closing the temp file: %s", strerror(errno));
            }
        }
        else
        {
            CIRCULAR_ERROR("Couldn't make a temp path: %s", strerror(errno));
        }
    }
    else
    {
        CIRCULAR_ERROR("The size of the circular buffer must be a multiple of 64kB and power of 2.");
    }
}

#else
#error NO PLATFORM DEFINED
#endif
