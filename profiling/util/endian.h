#pragma once

#if defined(__APPLE__) && defined(__MACH__)
#include <machine/endian.h>
#if __DARWIN_BYTE_ORDER == __DARWIN_BIG_ENDIAN
 #define htobe16(x) (x)
 #define htole16(x) __DARWIN_OSSwapInt16(x)
 #define be16toh(x) (x)
 #define le16toh(x) __DARWIN_OSSwapInt16(x)
 
 #define htobe32(x) (x)
 #define htole32(x) __DARWIN_OSSwapInt32(x)
 #define be32toh(x) (x)
 #define le32toh(x) __DARWIN_OSSwapInt32(x)
 
 #define htobe64(x) (x)
 #define htole64(x) __DARWIN_OSSwapInt64(x)
 #define be64toh(x) (x)
 #define le64toh(x) __DARWIN_OSSwapInt64(x)
#else __DARWIN_BYTE_ORDER == __DARWIN_LITTLE_ENDIAN
#define htobe16(x) __DARWIN_OSSwapInt16(x)
#define htole16(x) (x)
#define be16toh(x) __DARWIN_OSSwapInt16(x)
#define le16toh(x) (x)
#define htobe32(x) __DARWIN_OSSwapInt32(x)
#define htole32(x) (x)
#define be32toh(x) __DARWIN_OSSwapInt32(x)
#define le32toh(x) (x)
#define htobe64(x) __DARWIN_OSSwapInt64(x)
#define htole64(x) (x)
#define be64toh(x) __DARWIN_OSSwapInt64(x)
#define le64toh(x) (x)
#endif
#else
#include <endian.h>
#endif

