#include <sys/ksym.h>
#include <sys/kdbg.h>
#include <sys/kdbg_def_cmd.h>

#ifdef SPA_DEMO_ENABLE
#include "zfs_depend_impl.h"

KFUN_IMPORT(zfs, int, spa_open,
    (const char *, spa_t **, const void *), 2);
#define spa_open KSYM_REF(zfs,spa_open,2)

KFUN_IMPORT(zfs, void, spa_close, (spa_t *, const void *));
#define spa_close KSYM_REF(zfs,spa_close)

static void
zfs_show_imports(drv_inst_t *inst)
{
	// kdbg_print(inst, "spa_open=0x%lx\n", (unsigned long)spa_open);
	// kdbg_print(inst, "spa_close=0x%lx\n", (unsigned long)spa_close);
}

static void
print_tabs(drv_inst_t *inst, int tabs)
{
	while (tabs-- > 0)
		kdbg_print(inst, "\t");
}

static void
dump_vdev_(drv_inst_t *inst, vdev_t *vd, int depth)
{
	if (!vd)
		return;

	print_tabs(inst, depth);
	kdbg_print(inst, "|vd=0x%lx,%s\n",
	    (unsigned long)vd,
	    vd->vdev_path ? vd->vdev_path : "(null)");
	for (int i = 0; i < vd->vdev_children; i++)
		dump_vdev_(inst, vd->vdev_child[i], depth+1);
}

static void
dump_vdev(drv_inst_t *inst, vdev_t *vd)
{
	dump_vdev_(inst, vd, 0);
}

static void
zfs_label_main(drv_inst_t *inst, int argc, char *argv[])
{
	kdbg_print(inst, "This is a demo to call functions of zfs.\n");
	zfs_show_imports(inst);

	if (argc < 2) {
		kdbg_print(inst, "Usage: %s <poolname>\n", argv[0]);
		return;
	}

	// for (int i = 0; i < argc; i++)
		// kdbg_print(inst, "arg[%d]={%s}\n", i, argv[i]);

	spa_t *spa = NULL;
	const char *poolname = argv[1];

	int rc = spa_open(poolname, &spa, FTAG);
	if (rc) {
		kdbg_print(inst, "Open %s failure\n", poolname);
		return;
	}

	kdbg_print(inst, "spa=0x%lx\n", (unsigned long)spa);
	dump_vdev(inst, spa->spa_root_vdev);
	spa_close(spa, FTAG);
}
#endif // SPA_DEMO_ENABLE

KDBG_CMD_DEF(zfslabel, "", drv_inst_t *inst, int argc, char *argv[])
{
#ifdef SPA_DEMO_ENABLE
	zfs_label_main(inst, argc, argv);
#else
	kdbg_print(inst, "Please set environ varible spa_demo_enable=true "
	    "and recompile kdbg.ko\n");
#endif // SPA_DEMO_ENABLE
	return (0);
}
