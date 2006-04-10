// version.h

#define	QW_VERSION              2.40
#define FODQUAKE_VERSION        "0.1"

#ifdef _WIN32
#define QW_PLATFORM	"Win32"
#elif defined(linux)
#define QW_PLATFORM	"Linux"
#elif defined(__MORPHOS__)
#define QW_PLATFORM     "MorphOS"
#elif defined(__CYGWIN__)
#define QW_PLATFORM     "Cygwin"
#endif

#ifdef GLQUAKE
#define QW_RENDERER "GL"
#else
#define QW_RENDERER "Soft"
#endif

int build_number (void);
void CL_Version_f (void);
char *VersionString (void);
