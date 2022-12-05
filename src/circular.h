#include "circ_defines.h"

#if CIRCULAR_TYPES
#include <stdint.h>
#include "circ_types.h"
#endif

struct CircularBuffer
{
    void *base;
    u64 byteCount;
    volatile u64 readIndex;
    volatile u64 writeIndex;
    void *platform;
};

#define is_64k_mult(x)   (((x) & 0xFFFF) == 0)

CIRCULAR_INTERN void allocate_circular_buffer(CircularBuffer *buffer, u64 size);
CIRCULAR_INTERN void deallocate_circular_buffer(CircularBuffer *buffer);

CIRCULAR_INTERN void
clear(CircularBuffer *buffer)
{
    buffer->readIndex = 0;
    buffer->writeIndex = 0;
}

CIRCULAR_INTERN u64
get_size(CircularBuffer *buffer)
{
    CIRCULAR_ASSERT(buffer->writeIndex >= buffer->readIndex, "Circular buffer underflow, %lu >= %lu.", buffer->writeIndex, buffer->readIndex);
    u64 result = buffer->writeIndex - buffer->readIndex;
    return result;
}

CIRCULAR_INTERN u64
get_available_size(CircularBuffer *buffer)
{
    CIRCULAR_ASSERT(buffer->byteCount >= get_size(buffer), "Circular buffer overflow, %lu >= %lu.", buffer->byteCount, get_size(buffer));
    return buffer->byteCount - get_size(buffer);
}

CIRCULAR_INTERN void *
get_read_pointer(CircularBuffer *buffer)
{
    return (u8 *)buffer->base + buffer->readIndex;
}

CIRCULAR_INTERN void
read_advance(CircularBuffer *buffer, u64 byteCount)
{
    CIRCULAR_ASSERT(buffer->readIndex + byteCount <= buffer->writeIndex, "Circular buffer underflow, %lu <= %lu.", buffer->readIndex + byteCount, buffer->writeIndex);
    buffer->readIndex += byteCount;

    if (buffer->readIndex >= buffer->byteCount)
    {
        buffer->readIndex -= buffer->byteCount;
        // TODO(michiel): This should be atomic to be thread-safe
        buffer->writeIndex -= buffer->byteCount;

        CIRCULAR_LOG(Info, "Rewind %lu bytes.", buffer->byteCount);
    }

    CIRCULAR_LOG(Info, "Read %lu bytes.", byteCount);
}

CIRCULAR_INTERN void *
get_write_pointer(CircularBuffer *buffer)
{
    return (u8 *)buffer->base + buffer->writeIndex;
}

CIRCULAR_INTERN void
write_advance(CircularBuffer *buffer, u64 byteCount)
{
    CIRCULAR_ASSERT(buffer->writeIndex + byteCount <= 2 * buffer->byteCount, "Circular buffer overflow, %lu <= %lu.", buffer->writeIndex + byteCount, 2 * buffer->byteCount);
    // TODO(michiel): This should be atomic to be thread-safe
    buffer->writeIndex = buffer->writeIndex + byteCount;
    if ((buffer->writeIndex - buffer->readIndex) > buffer->byteCount)
    {
        u64 snoopBytes = (buffer->writeIndex - buffer->byteCount) - buffer->readIndex;
        CIRCULAR_LOG(Info, "Overflow, snooping up %lu bytes.", snoopBytes);
        read_advance(buffer, snoopBytes);
    }

    CIRCULAR_LOG(Info, "Wrote %lu bytes.", byteCount);
}
