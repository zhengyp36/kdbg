#include <sys/kdbg.h>
#include <sys/kdbg_impl.h>
#include <linux/kernel.h>

KDBG_CMD_DEF(echo, "[<args>] ...", drv_inst_t *inst, int argc, char *argv[])
{
	for (int i = 0; i < argc; i++)
		kdbg_print(inst, "[%d]={%s}\n", i, argv[i]);
	return (0);
}
