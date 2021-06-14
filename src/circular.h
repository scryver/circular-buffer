#include "circ_defines.h"

#if CIRCULAR_TYPES
#include <stdint.h>
#include "circ_types.h"
#endif

struct CircularBuffer
{
    void *base;
    u64 byteCount;
    u64 readIndex;
    u64 writeIndex;
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
    u64 result = buffer->writeIndex - buffer->readIndex;
    return result;
}

CIRCULAR_INTERN u64
get_available_size(CircularBuffer *buffer)
{
    return buffer->byteCount - get_size(buffer);
}

CIRCULAR_INTERN void *
get_write_pointer(CircularBuffer *buffer)
{
    return (u8 *)buffer->base + buffer->writeIndex;
}

CIRCULAR_INTERN void
write_advance(CircularBuffer *buffer, u64 byteCount)
{
    // TODO(michiel): Maybe add some debug checking here (writeIndex >= readIndex)
    // TODO(michiel): This should be atomic to be thread-safe
    buffer->writeIndex += byteCount;
}

CIRCULAR_INTERN void *
get_read_pointer(CircularBuffer *buffer)
{
    return (u8 *)buffer->base + buffer->readIndex;
}

CIRCULAR_INTERN void
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
