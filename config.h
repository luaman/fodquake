#define USE_ZLIB 1
#define USE_PNG 1
#if defined(__MORPHOS__) || defined(_WIN32)
#define USE_JPEG 0
#else
#define USE_JPEG 1
#endif

