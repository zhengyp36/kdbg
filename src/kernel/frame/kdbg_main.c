#include <sys/kdbg.h>
#include <sys/kdbg_impl.h>
#include <linux/delay.h>

int
kdbg_main(drv_inst_t *inst)
{
	usrcmd_t *uc = &inst->usrcmd;

	kdbg_print(inst, "argc=%d\n", uc->argc);
	for (int i = 0; i < uc->argc; i++) {
		kdbg_print(inst, "argv[%d]={%s}\n", i, uc->argv[i]);
		mdelay(100);
	}

	return (0);
}
