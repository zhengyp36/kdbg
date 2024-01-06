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

__attribute__((format(printf,3,4)))
int snprintf(char *, long, const char *, ...);

int strcmp(const char *, const char *);

struct dva;
struct vdev;

#define EXCLUDE_STRONG__BOOLEAN_T
typedef enum {B_FALSE, B_TRUE} boolean_t;

typedef boolean_t vdev_need_resilver_func_t(struct vdev *, const struct dva *,
    size_t psize, uint64_t phys_birth);

#include "zfs_depend.h"

#endif // ZFS_DEPEND_IMPL_H
