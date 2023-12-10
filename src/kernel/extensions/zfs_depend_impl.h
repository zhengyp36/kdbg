#ifndef ZFS_DEPEND_IMPL_H
#define ZFS_DEPEND_IMPL_H

#define EXCLUDE_BUILTIN_HEADER
//#define EXCLUDE_STRONG__STRUCT_CRYPTO_KEY

typedef signed char int8_t;
typedef short int16_t;
typedef int int32_t;
typedef long long int64_t;

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

typedef unsigned long intptr_t;
typedef unsigned long uintptr_t;
typedef unsigned long size_t;
typedef long ssize_t;

#define NULL ((void*)0)

#undef FTAG
#define	FTAG ((char *)(uintptr_t)__func__)

#include "zfs_depend.h"

#endif // ZFS_DEPEND_IMPL_H
