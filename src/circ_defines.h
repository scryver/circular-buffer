
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
