#ifndef _SYS_KDBG_TRACE_H
#define _SYS_KDBG_TRACE_H

#ifndef _KDBG_TRACE_ENABLE
#define kdbgtrace(name, ...)
#else // _KDBG_TRACE_ENABLE
#define kdbgtrace(name, ...) _kdbg_trace_def(name,##__VA_ARGS__)
#endif // _KDBG_TRACE_ENABLE

struct kdbg_trace_def;
struct kdbg_trace_imp;

typedef void (kdbg_trace_call_t)(const struct kdbg_trace_def *, ...);

typedef enum {
	KDBG_TRACE_DEF,
	KDBG_TRACE_IMP,
} kdbg_trace_type_t;

typedef struct kdbg_trace_head {
	struct kdbg_trace_head *next;
	const kdbg_trace_type_t type;
	int id;
} kdbg_trace_head_t;

typedef struct kdbg_trace_def {
	kdbg_trace_head_t	head;
	const char * const	name;
	struct kdbg_trace_imp *	impl;
	kdbg_trace_call_t *	call;
	const int		argc;
	const int		line;
	const char * const	file;
	const char * const	func;
} kdbg_trace_def_t;

#define _kdbg_trace_def(n,...)						\
	do {								\
		static kdbg_trace_def_t _kdbg_trace_name_(n,def,_) = {	\
			.head    = { .type = KDBG_TRACE_DEF },		\
			.name    = #n,					\
			.argc    = _kdbg_macro_argc(__VA_ARGS__),	\
			.file    = __FILE__,				\
			.func    = __func__,				\
			.line    = __LINE__				\
		};							\
		kdbg_trace_call_t *call =				\
		    _kdbg_trace_name_(n,def,_).call;			\
		kdbg_build_bug_on(					\
		    kdbg_offsetof(kdbg_trace_def_t,head.next));		\
		if (call)						\
			call(&_kdbg_trace_name_(n,def,_),		\
			    ##__VA_ARGS__);				\
	} while (0)

#define _kdbg_trace_name_(name,flag,mod)				\
	_kdbg_trace_##flag##_v1_##mod##_##name##_ff_aa_55_

#define _kdbg_macro_argc(...)						\
	_kdbg_macro_argc_(0, ##__VA_ARGS__,				\
	63, 62, 61, 60, 59, 58, 57, 56,					\
	55, 54, 53, 52, 51, 50, 49, 48,					\
	47, 46, 45, 44, 43, 42, 41, 40,					\
	39, 38, 37, 36, 35, 34, 33, 32,					\
	31, 30, 29, 28, 27, 26, 25, 24,					\
	23, 22, 21, 20, 19, 18, 17, 16,					\
	15, 14, 13, 12, 11, 10,  9,  8,					\
	 7,  6,  5,  4,  3,  2,  1,  0)
#define _kdbg_macro_argc_(_pad,						\
	_63, _62, _61, _60, _59, _58, _57, _56,				\
	_55, _54, _53, _52, _51, _50, _49, _48,				\
	_47, _46, _45, _44, _43, _42, _41, _40,				\
	_39, _38, _37, _36, _35, _34, _33, _32,				\
	_31, _30, _29, _28, _27, _26, _25, _24,				\
	_23, _22, _21, _20, _19, _18, _17, _16,				\
	_15, _14, _13, _12, _11, _10, _09, _08,				\
	_07, _06, _05, _04, _03, _02, _01, _00, ...) _00

#define kdbg_offsetof(s,t) ((unsigned long)&((s*)0)->t)
#define kdbg_build_bug_on(cond) (void)(sizeof(char[1-2*!!(cond)]))

#endif // _SYS_KDBG_TRACE_H
