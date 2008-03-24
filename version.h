// version.h

#define	QW_VERSION              2.40
#define FODQUAKE_VERSION        "0.1"

#ifdef _WIN32
#define QW_PLATFORM     "Win32"
#elif defined(linux)
#define QW_PLATFORM     "Linux"
#elif defined(__MORPHOS__)
#define QW_PLATFORM     "MorphOS"
#elif defined(__CYGWIN__)
#define QW_PLATFORM     "Cygwin"
#elif defined(__MACOSX__)
#define QW_PLATFORM     "MacOS X"
#elif defined(__FreeBSD__)
#define QW_PLATFORM     "FreeBSD"
#elif defined(__NetBSD__)
#define QW_PLATFORM     "NetBSD"
#elif defined(__OpenBSD__)
#define QW_PLATFORM     "OpenBSD"
#elif defined(GEKKO)
/* Not entirely true, but close enough for now. */
#define QW_PLATFORM     "Wii"
#endif

#ifdef GLQUAKE
#define QW_RENDERER "GL"
#else
#define QW_RENDERER "Soft"
#endif

void CL_Version_f (void);
char *VersionString (void);
