#include <sys/ksym.h>
#include <sys/kdbg.h>
#include <sys/kdbg_impl.h>
#include <linux/kernel.h>

#ifdef KSYM_DEMO_ENABLE

struct spa;

KFUN_IMPORT(zfs, int, spa_open, (const char *, struct spa **, const void *));
#define spa_open_0 KSYM_REF(zfs,spa_open)

KFUN_IMPORT(zfs, int, spa_open, (const char *, struct spa **, const void *), 1);
#define spa_open_1 KSYM_REF(zfs,spa_open,1)

#endif // KSYM_DEMO_ENABLE

KDBG_CMD_DEF(ksymdemo, "", drv_inst_t *inst, int argc, char *argv[])
{
#ifdef KSYM_DEMO_ENABLE
	kdbg_print(inst, "spa_open_0=0x%llx\n", (uint64_t)&spa_open_0);
	kdbg_print(inst, "spa_open_1=0x%llx\n", (uint64_t)&spa_open_1);
#else
	kdbg_print(inst, "Please set environ varible ksym_demo_enable=true "
	    "and recompile kdbg.ko\n");
#endif // KSYM_DEMO_ENABLE
	return (0);
}
