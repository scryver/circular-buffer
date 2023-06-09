#include "circ_defines.h"

#if CIRCULAR_TYPES
#include <stdint.h>
#include "circ_types.h"

#define is_pow2(x)       ((x) && (((x) & ((x) - 1)) == 0))
#define is_64k_mult(x)   (((x) & 0xFFFF) == 0)

#endif

struct CircularBuffer
{
    u8 *base;
    u64 indexMask;
    volatile u64 readIndex;
    volatile u64 writeIndex;
};

CIRCULAR_INTERN void allocate_circular_buffer(CircularBuffer *buffer, u64 size);
CIRCULAR_INTERN void deallocate_circular_buffer(CircularBuffer *buffer);

CIRCULAR_INTERN void
clear(CircularBuffer *buffer)
{
    buffer->readIndex = 0;
    buffer->writeIndex = 0;
}

CIRCULAR_INTERN u64
get_filled_size(CircularBuffer *buffer)
{
    u64 result = buffer->writeIndex - buffer->readIndex;
    return result;
}

CIRCULAR_INTERN u64
get_available_size(CircularBuffer *buffer)
{
    return (buffer->indexMask + 1) - get_filled_size(buffer);
}

CIRCULAR_INTERN void *
get_read_pointer(CircularBuffer *buffer)
{
    return buffer->base + (buffer->readIndex & buffer->indexMask);
}

CIRCULAR_INTERN void
read_advance(CircularBuffer *buffer, u64 byteCount)
{
    CIRCULAR_ASSERT(byteCount <= get_filled_size(buffer), "Circular buffer underflow, %lu <= %lu.", byteCount, get_filled_size(buffer));
    buffer->readIndex += byteCount;
    CIRCULAR_LOG(Info, "Read %lu bytes.", byteCount);
}

CIRCULAR_INTERN void *
get_write_pointer(CircularBuffer *buffer)
{
    return buffer->base + (buffer->writeIndex & buffer->indexMask);
}

CIRCULAR_INTERN void
write_advance(CircularBuffer *buffer, u64 byteCount)
{
    CIRCULAR_ASSERT(byteCount <= get_available_size(buffer), "Circular buffer overflow, %lu <= %lu.", byteCount, get_available_size(buffer));
    buffer->writeIndex += byteCount;
    CIRCULAR_LOG(Info, "Wrote %lu bytes.", byteCount);
}
