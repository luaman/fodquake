
#ifdef __GNUC__
#define PRINTFWARNING(x, y) __attribute__((format(printf, x, y)))
#endif

