#include <sys/ksym.h>
#include <sys/kdbg.h>
#include <sys/kdbg_def_cmd.h>
#include <linux/module.h>

#ifndef strequ
#define strequ(s1,s2) (!strcmp(s1,s2))
#endif

KDBG_CMD_DEF(mod, "<get|put|ref>", drv_inst_t *inst, int argc, char *argv[])
{
	if (argc != 2) {
		kdbg_cmd_usage();
	} else {
		if (strequ(argv[1], "get")) {
			int ref = kdbg_mod_get();
			kdbg_print(inst, "Inc mod ref: %d\n", ref);
		} else if (strequ(argv[1], "put")) {
			int ref = kdbg_mod_put();
			kdbg_print(inst, "Dec mod ref: %d\n", ref);
		} else if (strequ(argv[1], "ref")) {
			int ref = kdbg_mod_ref();
			kdbg_print(inst, "Current mod ref: %d\n", ref);
		} else {
			kdbg_cmd_usage();
		}
	}

	return (0);
}
