#include <sys/kdbg.h>
#include <sys/kdbg_impl.h>
#include <linux/sort.h>
#include <linux/kernel.h>

#define MAX_CMD_NUM 256
typedef struct {
	int		init;
	int		cnt;
	cmd_impl_t *	entry[MAX_CMD_NUM];
} cmd_table_t;

static cmd_table_t kdbg_usrcmd_table;

static int
cmd_ndx_cmp(const void *n1, const void *n2)
{
	cmd_impl_t **entry = &__kdbg_start_cmd + 1;
	cmd_impl_t *cmd1 = entry[*(const int *)n1];
	cmd_impl_t *cmd2 = entry[*(const int *)n2];

	int rc = cmd1->order - cmd2->order;
	return (rc ? rc : strcmp(cmd1->name, cmd2->name));
}

void
kdbg_cmd_table_init(void)
{
	cmd_table_t *tbl = &kdbg_usrcmd_table;
	if (tbl->init)
		return;

	cmd_impl_t **start = &__kdbg_start_cmd + 1, **stop = &__kdbg_stop_cmd;
	int cnt = stop - start;
	if (cnt == 0) {
		kdbg_log("Warning: No usr-cmd found.");
		goto done;
	} else if (cnt < 0) {
		kdbg_log("Error: __kdbg_start_cmd is behind __kdbg_stop_cmd.");
		goto done;
	} else if (cnt > MAX_CMD_NUM) {
		kdbg_log("Error: The number of usr-cmd %d more than %d.",
		    cnt, MAX_CMD_NUM);
		cnt = MAX_CMD_NUM;
	}

	int order[MAX_CMD_NUM];
	for (int i = 0; i < cnt; i++)
		order[i] = i;
	sort(order, cnt, sizeof(order[0]), cmd_ndx_cmp, NULL);

	for (int i = 0; i < cnt; i++)
		tbl->entry[i] = start[order[i]];
	tbl->cnt = cnt;

done:
	tbl->init = 1;
}

#define for_each_cmd(cmd)						\
	for (int _ = 0; _ < kdbg_usrcmd_table.cnt &&			\
	    !!(cmd = kdbg_usrcmd_table.entry[_]); _++)

static const cmd_impl_t *
lookup_cmd(const char *name)
{
	cmd_impl_t *cmd;
	for_each_cmd(cmd)
		if (strcmp(cmd->name, name) == 0)
			return (cmd);
	return (NULL);
}

KDBG_CMD_DEF_HIGH(help, "", drv_inst_t *inst, int argc, char *argv[])
{
	int show_low = 0;
	const char *prefix = "kdbg.sh access";
	kdbg_print(inst, "Usage:\n");

	cmd_impl_t *cmd;
	for_each_cmd(cmd) {
		if (!show_low && cmd->order >= KDBG_CMD_ORDER_LOW) {
			kdbg_print(inst, "\nSome demo commands below:\n");
			show_low = 1;
		}
		kdbg_print(inst, "       %s %s %s\n",
		    prefix, cmd->name, cmd->help ?: "");
	}

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

	return (def->entry(def, inst, uc->argc, uc->argv));
}
