#if defined(KDBG_TRACE_DEF_DEMO) || \
    defined(KDBG_TRACE_IMPL_DEMO) || \
    defined(KDBG_TRACE_IMPORT_DEMO)
#define _KDBG_TRACE_ENABLE
#endif

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
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

#define DEF_FUNC(type,name,addr) type name = (type)0x##addr##UL
#define DEF_TRACE(addr) (kdbg_trace_def_t*)0x##addr##UL

#define REG_TRACE(lmod, rmod, traces, trace_cnt)			\
	do {								\
		int errcnt = reg_fn(rmod, traces);			\
		printk("KDBG:KSYM: Register traces module"		\
		    "(%s->%s), total(%ld), errcnt(%d)",			\
		    lmod, rmod, trace_cnt, errcnt);			\
	} while (0)

typedef struct kdbg_trace_def kdbg_trace_def_t;
typedef int (*reg_trace_fn_t)(const char *, kdbg_trace_def_t**);

static void
do_trace(void)
{
	/* module: kdbg */ {
		/* import function: kdbg_trace_register */
		DEF_FUNC(reg_trace_fn_t, reg_fn, ffffffffc0ea1d00);

		static kdbg_trace_def_t *kdbg_kdbg_traces[] = {
			DEF_TRACE(ffffffffc0ea5320), /* kdbg->kdbg:TP1 */
			DEF_TRACE(ffffffffc0ea52e0), /* kdbg->kdbg:TP1 */
			DEF_TRACE(ffffffffc0ea52a0), /* kdbg->kdbg:TP2 */
			DEF_TRACE(ffffffffc0ea5260), /* kdbg->kdbg:TP2 */
			DEF_TRACE(0)
		};
		REG_TRACE("kdbg", "kdbg", kdbg_kdbg_traces,
		    ARRAY_SIZE(kdbg_kdbg_traces)-1);
	}

	/* module:??? */ {
		// ...
	}
}
#endif // KDBG_TRACE_REGISTER_BY_KSYM
