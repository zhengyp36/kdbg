#ifndef _SYS_KDBG_TRACE_IMPL_H
#define _SYS_KDBG_TRACE_IMPL_H

#include <sys/kdbg_trace.h>

/* This function is called during inserting kdbg.ko */
void kdbg_trace_init(void);

/* Register trace-points definitions
 *   @mod    : module's name
 *   @defs   : An array of pointers of trace-definition, ending with NULL
 *   @retval : The number of definitions failed to register
 *
 * Notes:
 *   The function is called by the module made by ksym.py
 */
int kdbg_trace_register(const char *mod, kdbg_trace_def_t **defs);

typedef struct kdbg_trace_def_list {
	kdbg_trace_def_t *	head;
	int			cnt;
	int			enable_cnt;
} kdbg_trace_def_list_t;

typedef struct kdbg_trace_imp {
	kdbg_trace_head_t		head;
	const char * const		mod;
	const char * const		name;
	kdbg_trace_def_list_t		defs;
	kdbg_trace_def_list_t		defs_hang;
	kdbg_trace_call_t * const	call;
	const int			argc;
	const int			line;
	const char * const		file;
	struct kdbg_trace_imp *		next_mod;
	struct kdbg_trace_imp *		curr_mod;
	int				registered;
} kdbg_trace_imp_t;

/* This function will never be called. But if 'head' was not the first
 * member of struct kdbg_trace_imp_t compiling would fail */
static inline void
__check_struct_head_kdbg_trace_imp_t__(void)
{
	kdbg_build_bug_on(kdbg_offsetof(kdbg_trace_imp_t,head.next));
}

/* Import module but not define no implement of trace-point */
#define KDBG_TRACE_IMPORT(mod) __KDBG_TRACE_DEFINE(mod,_,"",0,0)

/* Import module and define an implement of trace-point */
#define KDBG_TRACE_DEFINE(mod,name,...)					\
	static void _kdbg_trace_fun_name(mod,name)(			\
	    const kdbg_trace_def_t *, ...);				\
	__KDBG_TRACE_DEFINE(mod,name,#name,				\
	    _kdbg_trace_fun_name(mod,name),				\
	    _kdbg_macro_argc(__VA_ARGS__));				\
	static void _kdbg_trace_fun_name(mod,name)(			\
	    const kdbg_trace_def_t *_kdbg_trace_def_ptr_, ...)

#define __KDBG_TRACE_DEFINE(_mod,_name,strname,_call,_argc)		\
	static kdbg_trace_imp_t _kdbg_trace_imp_name(_mod,_name) = {	\
		.head		= { .type = KDBG_TRACE_IMP },		\
		.mod		= #_mod,				\
		.name		= strname,				\
		.call		= _call,				\
		.argc		= _argc,				\
		.file		= __FILE__,				\
		.line		= __LINE__				\
	};								\
	KDBG_TRACE_SEC("._kdbg.trace.impl") kdbg_trace_imp_t *		\
		_kdbg_trace_ptr_name(_mod,_name) =			\
		&_kdbg_trace_imp_name(_mod,_name)

#define KDBG_TRACE_SEC(sec) __attribute__((section(sec)))

#define KDBG_TRACE_SEC_START()						\
	KDBG_TRACE_SEC("._kdbg.trace.impl")				\
	kdbg_trace_imp_t *__kdbg_trace_impl_start
extern kdbg_trace_imp_t *__kdbg_trace_impl_start;

#define KDBG_TRACE_SEC_STOP()						\
	KDBG_TRACE_SEC("._kdbg.trace.impl")				\
	kdbg_trace_imp_t *__kdbg_trace_impl_stop
extern kdbg_trace_imp_t *__kdbg_trace_impl_stop;

#define _kdbg_trace_fun_name(mod,name) _kdbg_trace_name_(name,fun,mod)
#define _kdbg_trace_imp_name(mod,name) _kdbg_trace_name_(name,imp,mod)
#define _kdbg_trace_ptr_name(mod,name) _kdbg_trace_name_(name,ptr,mod)

#endif // _SYS_KDBG_TRACE_IMPL_H
