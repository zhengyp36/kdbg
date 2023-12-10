#ifndef _SYS_KSYM_H
#define _SYS_KSYM_H

/*
 * Note: module-name of symbols of kernel is _ such as vfs_open, do_unlinkat ...
 *
 * Example-1:
 *     KFUN_IMPORT(_, int, vfs_open,
 *                (const struct path *, struct file *, const struct cred *));
 *     #define vfs_open KSYM_REF(_,vfs_open)
 *
 * Example-2:
 *     KVAR_IMPORT(zfs, struct avl_tree, spa_namespace_avl);
 *     #define spa_namespace_avl KSYM_REF(zfs,spa_namespace_avl)
 *
 * Example-3:
 *     KFUN_IMPORT(zfs, int, spa_open,
 *                (const char *, struct spa **, const void *));
 *     #define spa_open KSYM_REF(zfs,spa_open)
 *
 */

#define KSYM_REF(mod,name,...) (*KSYM_NAME(mod,name,##__VA_ARGS__))
#define KSYM_NAME(mod,name,...) KSYM_NAME_(mod,name,##__VA_ARGS__,0)
#define KSYM_NAME_(mod,name,ndx,...) \
	__ksym_1537_##name##_2489_mod_##mod##_ndx_##ndx##_ffff_

#define KVAR_IMPORT(mod,type,name,...)					\
	__attribute__((section("._kdbg.ksym")))				\
	type *KSYM_NAME(mod,name,##__VA_ARGS__)

#define KFUN_IMPORT(mod,type,name,proto,...)				\
	__attribute__((section("._kdbg.ksym")))				\
	type (*KSYM_NAME(mod,name,##__VA_ARGS__))proto

#define KSYM_SEC_START()						\
	__attribute__((section("._kdbg.ksym")))				\
	void *__kdbg_ksym_start = 0

#define KSYM_SEC_STOP()							\
	__attribute__((section("._kdbg.ksym")))				\
	void *__kdbg_ksym_stop = 0

__attribute__((weak)) int __kdbg_ksym_imported;

extern void *__kdbg_ksym_start;
extern void *__kdbg_ksym_stop;

static inline int
ksym_imported(void)
{
	return (__kdbg_ksym_imported ||
	    &__kdbg_ksym_start + 1 == &__kdbg_ksym_stop);
}

#endif // _SYS_KSYM_H
