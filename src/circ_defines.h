
#ifndef LINUX_BUILD
#define LINUX_BUILD 0
#endif

#ifndef WINDOWS_BUILD
#define WINDOWS_BUILD 0
#endif

#ifndef APPLE_BUILD
#define APPLE_BUILD 0
#endif

#ifndef CIRCULAR_TYPES
#define CIRCULAR_TYPES 1
#endif

#if !LINUX_BUILD && !WINDOWS_BUILD && !APPLE_BUILD
#error Specify a platform to build for
#endif

#ifndef CIRCULAR_INTERN
#define CIRCULAR_INTERN static
#endif

#ifndef CIRCULAR_LOG
#define CIRCULAR_LOG(lvl, msg, ...)    fprintf(stdout, #lvl ": " msg "\n", ## __VA_ARGS__)
#endif

#ifndef CIRCULAR_ERROR
#define CIRCULAR_ERROR(msg, ...)       fprintf(stderr, "Error: " msg "\n", ## __VA_ARGS__)
#endif

#ifndef CIRCULAR_ASSERT
#define CIRCULAR_ASSERT(x, msg, ...)  \
((x) ? (void)0 : (CIRCULAR_ERROR("Assertion failed: " #x ", " #msg "\n", ## __VA_ARGS__), (void)(*(volatile int *)0 = 0)))
#endif
