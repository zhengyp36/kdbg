#include <sys/ksym.h>
#include <sys/kdbg.h>
#include <sys/kdbg_def_cmd.h>

#ifdef SPA_DEMO_ENABLE

#include "zfs_depend_impl.h"

typedef int abd_iter_func_t(void *buf, size_t len, void *priv);

KFUN_IMPORT(zfs, int, abd_iterate_func,
    (abd_t *, size_t, size_t, abd_iter_func_t *, void *));
#define abd_iterate_func KSYM_REF(zfs,abd_iterate_func)

KFUN_IMPORT(zfs, int, spa_keystore_dsl_key_hold_impl,
    (spa_t *, uint64_t, const void *, dsl_crypto_key_t **));
#define spa_keystore_dsl_key_hold_impl \
    KSYM_REF(zfs,spa_keystore_dsl_key_hold_impl)

static void
spa_demo(drv_inst_t *inst, int argc, char *argv[])
{
	kdbg_print(inst, "This is a demo to call functions of zfs.\n");
	kdbg_print(inst, "abd_iterate_func=0x%lx\n", (size_t)&abd_iterate_func);
	kdbg_print(inst, "spa_keystore_dsl_key_hold_impl=0x%lx\n",
	    (size_t)&spa_keystore_dsl_key_hold_impl);
}
#endif // SPA_DEMO_ENABLE

KDBG_CMD_DEF_LOW(spademo, "", drv_inst_t *inst, int argc, char *argv[])
{
#ifdef SPA_DEMO_ENABLE
	spa_demo(inst, argc, argv);
#else
	kdbg_print(inst, "Please set environ varible spa_demo_enable=true "
	    "and recompile kdbg.ko\n");
#endif // SPA_DEMO_ENABLE
	return (0);
}
