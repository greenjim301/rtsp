#ifndef _CYGWIN_INTTYPES_H
#define _CYGWIN_INTTYPES_H
/* /usr/include/inttypes.h for CYGWIN
 * Copyleft 2001-2002 by Felix Buenemann
 * <atmosfear at users.sourceforge.net>
 */
#include <limits.h>

typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short int int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
#ifndef WIN64
typedef signed int intptr_t;
typedef unsigned int uintptr_t;
#endif

#ifdef _MSC_VER
 typedef __int64 int64_t;
 typedef unsigned __int64 uint64_t;
#else
 typedef long long int64_t;
 typedef unsigned long long uint64_t;
#endif

#ifndef WIN64
 #define __WORDSIZE 32
#else
 #define __WORDSIZE 64
#endif
/*
typedef unsigned short UINT16;
typedef signed short INT16;
typedef unsigned char UINT8;
typedef unsigned int UINT32;
typedef uint64_t UINT64;
typedef signed char INT8;
typedef signed int INT32;
typedef int64_t INT64;

typedef long _ssize_t;
typedef _ssize_t ssize_t;
*/

#define PRId64 "I64d"
#define PRIx64 "I64x"

#ifndef M_PI
#define M_PI    3.14159265358979323846
#endif

#ifndef M_E
#define M_E		2.718281828459
#endif

#ifndef _I64_MIN
#define _I64_MIN (-9223372036854775807LL-1)
#endif

#ifndef _I64_MAX
#define _I64_MAX 9223372036854775807LL
#endif

#ifndef _UI32_MAX
#define _UI32_MAX 0xffffffffL
#endif

#ifndef INT32_MIN
#define INT32_MIN (-2147483647 - 1)
#endif

#ifndef INT32_MAX
#define INT32_MAX 2147483647
#endif

#ifndef INT_MIN
#define INT_MIN	INT32_MIN
#endif

#ifndef INT_MAX
#define INT_MAX	INT32_MAX
#endif

#ifndef INT16_MIN
#define INT16_MIN       (-0x7fff-1)
#endif

#ifndef INT16_MAX
#define INT16_MAX       0x7fff
#endif

#endif