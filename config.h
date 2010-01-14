#ifndef _WIN32
#define USE_ZLIB 1
#define USE_PNG 1
#ifdef __MORPHOS__
#define USE_JPEG 0
#else
#define USE_JPEG 1
#endif
#endif

