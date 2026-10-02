#ifndef CONFIG_H
#define CONFIG_H

#ifndef inline
#define inline __inline
#endif

#if _M_IX86_FP >= 1 || defined(_M_X64)
#define _USE_SSE
#endif

#if _M_IX86_FP >= 2 || defined(_M_X64)
#define _USE_SSE2
#endif

#ifndef _USE_SSE
#  define USE_ALLOCA
#endif

#ifndef FIXED_POINT
#  ifndef FLOATING_POINT
#    define FLOATING_POINT
#  endif
#  define USE_SMALLFT
#else
#  define USE_KISS_FFT
#endif

#define EXPORT

#include <math.h>

#ifndef SATURATE32PSHR
#define SATURATE32PSHR(x,shift,a) (x)
#endif

#ifndef WORD2INT
#define WORD2INT(x) ((x) < -32767.5f ? -32768 : ((x) > 32766.5f ? 32767 : (spx_int16_t)floor(.5 + (x))))
#endif

#define SPEEX_MAJOR_VERSION 1
#define SPEEX_MINOR_VERSION 2
#define SPEEX_MICRO_VERSION 1
#define SPEEX_EXTRA_VERSION ""
#define SPEEX_VERSION "1.2.1"

#endif // CONFIG_H
