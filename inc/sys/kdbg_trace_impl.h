#ifndef _SYS_KDBG_TRACE_IMPL_H
#define _SYS_KDBG_TRACE_IMPL_H

#include <sys/kdbg_trace.h>

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

/* Unregister all trace-points definitions
 *   @retval : how many trace-points disabled at this time
 *
 * Note:
 *   It's necessary to unregister all trace-points definitions registered before
 * because some modules may be removed already and It is unsafe to access these
 * definitions. So call it in ksym.py before register new definitions.
 */
int kdbg_trace_unregister_all(void);

/*
 * Call the function to number every trace-definition. It's necessary for
 * ksym.py after calling the kdbg_trace_register
 */
void kdbg_trace_update_order(void);

typedef struct kdbg_trace_def_list {
	kdbg_trace_def_t *head;
	int cnt;
} kdbg_trace_def_list_t;

typedef struct kdbg_trace_imp {
	const char * const		mod;
	const char * const		name;
	kdbg_trace_def_list_t		defs;
	kdbg_trace_def_list_t		defs_hang;
	kdbg_trace_call_t * const	call;
	const int			argc;
	const int			line;
	const char * const		file;
	struct kdbg_trace_imp *		next;
	struct kdbg_trace_imp *		next_mod;
	struct kdbg_trace_imp *		curr_mod;
	int				traceid;
} kdbg_trace_imp_t;

#define kdbg_for_each_trace_impl(impl)					\
	for (kdbg_trace_imp_t **_ = &__kdbg_trace_impl_start + 1;	\
	    _ < &__kdbg_trace_impl_stop && ((impl) = *_); _++)

/* Import module but not define no implement of trace-point */
#define KDBG_TRACE_IMPORT(mod) __KDBG_TRACE_DEFINE(mod,_,0,0)
#define KDBG_TRACE_INVALID_NAME "_"

/* Import module and define an implement of trace-point */
#define KDBG_TRACE_DEFINE(mod,name,...)					\
	static void _kdbg_trace_fun_name(mod,name)(			\
	    const kdbg_trace_def_t *, ...);				\
	__KDBG_TRACE_DEFINE(mod, name,					\
	    _kdbg_trace_fun_name(mod,name),				\
	    _kdbg_macro_argc(__VA_ARGS__));				\
	static void _kdbg_trace_fun_name(mod,name)(			\
	    const kdbg_trace_def_t *_kdbg_trace_def_ptr_, ...)

#define __KDBG_TRACE_DEFINE(_mod,_name,_call,_argc)			\
	static kdbg_trace_imp_t _kdbg_trace_imp_name(_mod,_name) = {	\
		.mod	= #_mod,					\
		.name	= #_name,					\
		.call	= _call,					\
		.argc	= _argc,					\
		.file	= __FILE__,					\
		.line	= __LINE__					\
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
