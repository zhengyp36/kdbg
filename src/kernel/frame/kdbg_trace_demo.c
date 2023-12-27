#define _KDBG_TRACE_ENABLE
#define KDBG_TRACE_DEF_DEMO
#define KDBG_TRACE_IMPL_DEMO
#define KDBG_TRACE_IMPORT_DEMO

#include <sys/kdbg.h>
#include <sys/kdbg_impl.h>
#include <sys/kdbg_trace_impl.h>

#ifdef KDBG_TRACE_IMPORT_DEMO
KDBG_TRACE_IMPORT(zfs);
KDBG_TRACE_IMPORT(kdbg1);
#endif // KDBG_TRACE_IMPORT_DEMO

#ifdef KDBG_TRACE_DEF_DEMO
void
kdbg_trace_def_demo(kdbg_trace_def_t **defs)
{
	kdbgtrace(TP1, defs, kdbg_trace_register, 0);
	kdbgtrace(TP1, defs, kdbg_trace_register, 1);
	kdbgtrace(TP2, defs, kdbg_trace_register, 0);
	kdbgtrace(TP2, defs, kdbg_trace_register, 1);
}
#endif // KDBG_TRACE_DEF_DEMO

#ifdef KDBG_TRACE_IMPL_DEMO
KDBG_TRACE_DEFINE(kdbg,  TP1, kdbg_trace_def_t **defs, void *pfn, int id) {}
KDBG_TRACE_DEFINE(kdbg,  TP2, kdbg_trace_def_t **defs, void *pfn, int id) {}
KDBG_TRACE_DEFINE(kdbg1, TP1, kdbg_trace_def_t **defs, void *pfn, int id) {}
KDBG_TRACE_DEFINE(kdbg1, TP2, kdbg_trace_def_t **defs, void *pfn, int id) {}
KDBG_TRACE_DEFINE(kdbg2, TP1, kdbg_trace_def_t **defs, void *pfn, int id) {}
KDBG_TRACE_DEFINE(kdbg2, TP2, kdbg_trace_def_t **defs, void *pfn, int id) {}
#endif // KDBG_TRACE_IMPL_DEMO

#ifdef KDBG_TRACE_REGISTER_BY_KSYM
/*
 * # cat /proc/kallsyms | grep '\[kdbg\]' | grep -e kdbg_trace_register -e kdbg_trace_unregister_all -e _kdbg_trace_def_
 * ffffffffc0fde320 d _kdbg_trace_def_v1___TP1_ff_aa_55_.11356	[kdbg]
 * ffffffffc0fde2e0 d _kdbg_trace_def_v1___TP1_ff_aa_55_.11360	[kdbg]
 * ffffffffc0fde2a0 d _kdbg_trace_def_v1___TP2_ff_aa_55_.11363	[kdbg]
 * ffffffffc0fde260 d _kdbg_trace_def_v1___TP2_ff_aa_55_.11366	[kdbg]
 * ffffffffc0fdb0d0 t kdbg_trace_register	[kdbg]
 * ffffffffc0fdb220 t kdbg_trace_unregister_all	[kdbg]
 */

#include <linux/kernel.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

#define DEF_TRACE(addr) (kdbg_trace_def_t*)0x##addr

#define DEF_FUNC(ret_type, func, proto, addr)	\
	ret_type (*func) proto = (typeof(func))0x##addr

typedef struct kdbg_trace_def kdbg_trace_def_t;

/* import:kdbg_trace_register */
DEF_FUNC(int, trace_register,
	(const char *, kdbg_trace_def_t **), ffffffffc0fdb0d0);

/* import:kdbg_trace_unregister_all */
DEF_FUNC(void, trace_unregister_all, (void), ffffffffc0fdb220);

void
do_register(void)
{
	/* module:kdbg */
	static kdbg_trace_def_t *kdbg_traces[] = {
		DEF_TRACE(ffffffffc0fde320),
		DEF_TRACE(ffffffffc0fde2e0),
		DEF_TRACE(ffffffffc0fde2a0),
		DEF_TRACE(ffffffffc0fde260),
		0,
	};
	int kdbg_errcnt = trace_register("kdbg", kdbg_traces);
	printk("Register kdbg_traces mod(kdbg), total(%ld), errcnt(%d)",
	    ARRAY_SIZE(kdbg_traces), kdbg_errcnt);
}

#endif // KDBG_TRACE_REGISTER_BY_KSYM
