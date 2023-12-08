#include <sys/kdbg.h>
#include <sys/kdbg_impl.h>
#include <linux/kernel.h>

#define for_each_cmd(cmd) \
	for (cmd = &__kdbg_start_cmd+1; cmd < &__kdbg_stop_cmd; cmd++)

static const cmd_impl_t *
lookup_cmd(const char *name)
{
	cmd_impl_t **cmd;
	for_each_cmd(cmd)
		if (strcmp((*cmd)->name, name) == 0)
			return (*cmd);
	return (NULL);
}

KDBG_CMD_DEF(help, "", drv_inst_t *inst, int argc, char *argv[])
{
	const char *prefix = "kdbg.sh access";
	kdbg_print(inst, "Usage:\n");

	cmd_impl_t **cmd;
	for_each_cmd(cmd)
		kdbg_print(inst, "       %s %s %s\n",
		    prefix, (*cmd)->name, (*cmd)->help ?: "");

	return (0);
}

int
kdbg_main(drv_inst_t *inst)
{
	usrcmd_t *uc = &inst->usrcmd;

	const char *name = uc->argc > 0 ? uc->argv[0] : "help";
	const cmd_impl_t *def = lookup_cmd(name);
	if (!def) {
		kdbg_print(inst, "Error: *** invalid cmd(%s)\n", name);
		return (-1);
	}

	return (def->entry(inst, uc->argc, uc->argv));
}
