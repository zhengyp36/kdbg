#include <stdarg.h>
#include <sys/kdbg.h>
#include <sys/kdbg_impl.h>
#include <sys/kdbg_def_cmd.h>
#include <sys/kdbg_trace_impl.h>
#include <linux/slab.h>
#include <linux/sort.h>
#include <linux/kernel.h>
#include <linux/module.h>

#ifndef MAX
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#endif

typedef struct kdbg_trace_imp_array {
	kdbg_trace_imp_t **	arr;
	int			cnt;
} imp_arr_t;

typedef struct {
	struct mutex		mtx;
	kdbg_trace_imp_t *	head;
	int			trace_cnt;
	int			inited;
} trace_mgr_t;

#define mgr_lock(mgr)   mutex_lock(&(mgr)->mtx)
#define mgr_unlock(mgr) mutex_unlock(&(mgr)->mtx)

enum {
	VISIT_CURR_IMP,
	VISIT_NEXT_IMP,
	VISIT_NEXT_MOD
};

typedef struct trace_iter {
	kdbg_trace_imp_t *	imp;
	kdbg_trace_head_t *	trace;
	int			hang;
	int			done;
} trace_iter_t;

#define for_each_trace(mgr, iter, trace)				\
	for (trace_iter_init(iter,mgr);					\
	    !!((trace) = trace_iter_retrieve(iter));			\
	    trace_iter_advance(iter))

typedef struct {
	int ok_cnt;
	int hang_cnt;
	int err_cnt;
} register_stat_t;

enum {
	TRACE_CMD_DUMP,
	TRACE_CMD_ENABLE,
	TRACE_CMD_DISABLE,
	TRACE_CMD_CLEANUP,
	TRACE_CMD_NUM
};

typedef struct trace_args {
	int	cmd_no; // TRACE_CMD_XXX
	int	traces_all;
	int	traces_ndx;
	int	traces_cnt;
	char **	traces_arr;
} trace_args_t;

typedef struct trace_array {
	trace_mgr_t *		mgr;
	drv_inst_t *		inst;
	kdbg_trace_head_t **	arr;
	int			is_empty;
	int			arr_cnt;
	int			max_arr_cnt;
	int			locked;
	int			err;
} trace_arr_t;

enum {
	DUMP_TRACE_ID,
	DUMP_NAME,
	DUMP_ARGC,
	DUMP_ENABLE,
	DUMP_FUNC,
	DUMP_LINE,
	DUMP_FILE,
	DUMP_MAX_ENTRY
};

#define DUMP_TITLE_TRACE_ID "TraceID"
#define DUMP_TITLE_NAME     "Name"
#define DUMP_TITLE_ARGC     "Argcnt"
#define DUMP_TITLE_ENABLE   "Enable"
#define DUMP_TITLE_FUNC     "Function"
#define DUMP_TITLE_LINE     "Line"
#define DUMP_TITLE_FILE     "Source"

#define DUMP_BUF_LENGTH 512
#define DUMP_NAME_BUF_LEN 128
typedef struct dump_ctx {
	drv_inst_t *	inst;
	trace_arr_t *	trace_arr;
	const char *	sep;
	const char *	invstr;
	int		stat_max_len;
	int		max_len;
	int		dump_cnt;
	uint8_t		len    [DUMP_MAX_ENTRY];
	char		namebuf[DUMP_NAME_BUF_LEN];
	char		buffer [DUMP_BUF_LENGTH];
	char		title  [DUMP_BUF_LENGTH];
	char		def_fmt[DUMP_BUF_LENGTH];
	char		imp_fmt[DUMP_BUF_LENGTH];
} dump_ctx_t;

__attribute__((format(printf,3,4)))
static void dump_stat_len(dump_ctx_t *, int, const char *, ...);

#define bool2str(v) (!!(v) ? "true" : "false")

static trace_mgr_t trace_mgr = {
	.mtx = __MUTEX_INITIALIZER(trace_mgr.mtx)
};

/*
 * This trace-imp is only used to manage trace-definitions of a module,
 * but it is not a trace-imp itself and can not be registered by a trace-def
 */
#define TRACE_NAME_FOR_MOD ""
static inline int
imp_is_mod_only(const char *name)
{
	return (!strcmp(name, TRACE_NAME_FOR_MOD));
}

static int
imp_cmp(const void *i1, const void *i2)
{
	const kdbg_trace_imp_t *imp1 = *(const kdbg_trace_imp_t **)i1;
	const kdbg_trace_imp_t *imp2 = *(const kdbg_trace_imp_t **)i2;

	int ret = strcmp(imp1->mod, imp2->mod);
	if (ret)
		return (ret);
	else if (imp_is_mod_only(imp1->name))
		return (1);
	else if (imp_is_mod_only(imp2->name))
		return (-1);
	else
		return (strcmp(imp1->name, imp2->name));
}

static void
get_imp_arr(imp_arr_t *arr)
{
	arr->arr = &__kdbg_trace_impl_start + 1;
	arr->cnt = &__kdbg_trace_impl_stop - arr->arr;
	if (arr->cnt > 0)
		sort(arr->arr, arr->cnt, sizeof(arr->arr[0]), imp_cmp, NULL);
	kdbg_log("There are %d trace-imp-definitions found", arr->cnt);
}

static void
iterate_imp(trace_iter_t *iter, kdbg_trace_imp_t *imp, int iterate_type)
{
	while (imp) {
		if (iterate_type != VISIT_CURR_IMP) {
			int curr_iterate_type = iterate_type;
			iterate_type = VISIT_CURR_IMP;
			if (curr_iterate_type == VISIT_NEXT_IMP)
				goto VISIT_NEXT_IMP;
			else if (curr_iterate_type == VISIT_NEXT_MOD)
				goto VISIT_NEXT_MOD;
			// else
				// goto VISIT_CURR_IMP;
		}

// VISIT_CURR_IMP:
		// visit defs of curr_imp
		if (imp->defs.head) {
			iter->imp = imp;
			iter->trace = &imp->defs.head->head;
			break;
		}

		// visit curr_imp itself
		if (!imp_is_mod_only(imp->name)) {
			iter->imp = imp;
			iter->trace = &imp->head;
			break;
		}

VISIT_NEXT_IMP:
		// visit next_imp of curr_mod
		if (imp->head.next) {
			imp = (kdbg_trace_imp_t *)imp->head.next;
			continue;
		}

		// it's last imp in curr_mod, so visit defs_hang now

		// Check for safety even though It's impossible
		if (!imp->curr_mod) {
			iter->done = 1;
			break;
		}

		// visit defs_hang of curr_mod
		if (imp->curr_mod->defs_hang.head) {
			iter->hang = 1;
			iter->imp = imp;
			iter->trace = &imp->curr_mod->defs_hang.head->head;
			break;
		}

VISIT_NEXT_MOD:
		// visit next_mod
		if (imp->curr_mod->next_mod) {
			imp = imp->curr_mod->next_mod;
			continue;
		}

		// No next_mod and iterate done
		iter->done = 1;
		break;
	}

	if (!imp)
		iter->done = 1;
}

static void
trace_iter_init(trace_iter_t *iter, trace_mgr_t *mgr)
{
	iter->imp = NULL;
	iter->trace = NULL;
	iter->hang = 0;
	iter->done = 0;
	iterate_imp(iter, mgr->head, VISIT_CURR_IMP);
}

static inline kdbg_trace_head_t *
trace_iter_retrieve(trace_iter_t *iter)
{
	return (!iter->done ? iter->trace : NULL);
}

static void
trace_iter_advance(trace_iter_t *iter)
{
	if (iter->done) {
		return;
	} else if (iter->trace->type == KDBG_TRACE_DEF) {
		if (iter->trace->next) {
			iter->trace = iter->trace->next;
		} else if (!iter->hang) {
			iterate_imp(iter, iter->imp, VISIT_NEXT_IMP);
		} else {
			iter->hang = 0;
			iterate_imp(iter, iter->imp, VISIT_NEXT_MOD);
		}
	} else { // iter->trace->type == KDBG_TRACE_IMP
		iterate_imp(iter, iter->imp, VISIT_NEXT_IMP);
	}
}

/* Note that where_mod MUST be initialized */
static kdbg_trace_imp_t *
imp_lookup(trace_mgr_t *mgr, const char *mod, const char *name,
    kdbg_trace_imp_t ***where, kdbg_trace_imp_t ***where_mod)
{
	kdbg_trace_imp_t **iter = &mgr->head;

	// lookup by module
	if (where_mod && *where_mod && **where_mod &&
	    !strcmp((**where_mod)->mod, mod)) {
		iter = *where_mod;
	} else {
		while (*iter && strcmp((*iter)->mod, mod))
			iter = &(*iter)->next_mod;
		if (where_mod)
			*where_mod = iter;
	}

	// lookup by trace-name
	if (!imp_is_mod_only(name))
		while (*iter && strcmp((*iter)->name, name))
			iter = (kdbg_trace_imp_t **)&(*iter)->head.next;
	if (where)
		*where = iter;

	return (*iter);
}

static void
trace_order(trace_mgr_t *mgr)
{
	trace_iter_t iter;
	kdbg_trace_head_t *trace;

	mgr->trace_cnt = 0;
	for_each_trace(mgr, &iter, trace) {
		trace->id = ++(mgr->trace_cnt);
	}
}

static void
trace_mgr_init(trace_mgr_t *mgr)
{
	if (mgr->inited)
		return;

	imp_arr_t arr;
	get_imp_arr(&arr);

	const char *info;
	kdbg_trace_imp_t *imp, **where, **where_mod = NULL;
	for (int i = 0; i < arr.cnt; i++) {
		imp = arr.arr[i];
		if (imp_lookup(mgr, imp->mod, imp->name, &where, &where_mod)) {
			info = "exist";
		} else if (imp_is_mod_only(imp->name) && *where_mod) {
			info = "skip module-only";
		} else {
			info = "insert";
			imp->curr_mod = *where_mod ? *where_mod : imp;
			*where = imp;
		}
		kdbg_log("%s: mod=%s, name=%s", info, imp->mod, imp->name);
	}

	trace_order(mgr);
	mgr->inited = 1;
}

static int
add_trace(kdbg_trace_def_list_t *list,
    kdbg_trace_def_t *def, kdbg_trace_imp_t *imp)
{
	kdbg_trace_def_t **iter = &list->head;
	while (!!*iter) {
		if (*iter == def)
			return (0);
		iter = (kdbg_trace_def_t **)&(*iter)->head.next;
	}

	def->impl = imp;
	*iter = def;
	list->cnt++;
	return (1);
}

static inline void
enable_trace(kdbg_trace_def_t *def)
{
	// The def must be in the list of def->impl.defs but here not check it
	if (!def->call && def->impl && def->impl->call) {
		def->call = def->impl->call;
		def->impl->defs.enable_cnt++;
	}
}

static inline void
disable_trace(kdbg_trace_def_t *def)
{
	if (def->call && def->impl && def->impl->defs.enable_cnt > 0) {
		def->impl->defs.enable_cnt--;
		def->call = NULL;
	}
}

static void
disable_all_traces(kdbg_trace_imp_t *imp)
{
	kdbg_trace_def_t *def = imp->defs.head;
	while (def && imp->defs.enable_cnt > 0) {
		if (def->call)
			disable_trace(def);
		def = (kdbg_trace_def_t *)def->head.next;
	}
}

static inline int
def_list_clear(kdbg_trace_def_list_t *list)
{
	int cnt = list->cnt;
	list->head = NULL;
	list->cnt = list->enable_cnt = 0;
	return (cnt);
}

static int
clear_all_traces(kdbg_trace_imp_t *imp)
{
	disable_all_traces(imp);
	return (def_list_clear(&imp->defs) + def_list_clear(&imp->defs_hang));
}

static void
register_traces(trace_mgr_t *mgr, const char *mod, kdbg_trace_def_t **defs,
    register_stat_t *r)
{
	kdbg_trace_def_t **pdef, *def;
	kdbg_trace_imp_t *imp, **where, **where_mod = NULL;
	const char *info;

#define FMT_TRACE_DEF "name=%s,call=0x%lx,argc=%d,file=%s,func=%s,line=%d"
#define VAL_TRACE_DEF(def)						\
	def->name, (unsigned long)def->call, def->argc, def->file,	\
	    def->func, def->line

	r->ok_cnt = r->hang_cnt = r->err_cnt = 0;
	for (pdef = defs; !!(def = *pdef); pdef++) {
		imp = imp_lookup(mgr, mod, def->name, &where, &where_mod);
		if (imp) {
			r->ok_cnt += add_trace(&imp->defs, def, imp);
			info = "normal";
		} else if (!!where_mod && !!(imp = *where_mod)) {
			r->hang_cnt += add_trace(&imp->defs_hang, def, imp);
			info = "hang";
		} else {
			kdbg_log("Register trace-def failure because module"
			    "(%s) is not found: "FMT_TRACE_DEF,
			    mod, VAL_TRACE_DEF(def));
			r->err_cnt++;
			break;
		}
		kdbg_log("Register %s trace-def: "FMT_TRACE_DEF,
		    info, VAL_TRACE_DEF(def));
	}

	if (r->ok_cnt > 0 || r->hang_cnt > 0) {
		imp = *where_mod;
		if (!imp->hold_mod) {
			// TODO: do not hold current module itself
			// TODO: hold the reference of the module
			imp->hold_mod = 1;
		}
	}

#undef FMT_TRACE_DEF
#undef VAL_TRACE_DEF
}

static void
dump_single_trace(dump_ctx_t *ctx, kdbg_trace_head_t *trace)
{
	drv_inst_t *inst = ctx->inst;
	if (trace->type == KDBG_TRACE_DEF) {
		kdbg_trace_def_t *def = (kdbg_trace_def_t *)trace;
		snprintf(ctx->namebuf, sizeof(ctx->namebuf), "%s:%s",
		    def->impl->mod, def->name);
		kdbg_print(inst, "Details of Single-Trace [DEF]:\n");
		kdbg_print(inst, "------------------------------\n");
		kdbg_print(inst, "[%-8s] %d\n", DUMP_TITLE_TRACE_ID, trace->id);
		kdbg_print(inst, "[%-8s] %s\n", DUMP_TITLE_NAME,  ctx->namebuf);
		kdbg_print(inst, "[%-8s] %d\n", DUMP_TITLE_ARGC,     def->argc);
		kdbg_print(inst, "[%-8s] %s\n", DUMP_TITLE_ENABLE,
		                                           bool2str(def->call));
		kdbg_print(inst, "[%-8s] %s:%d\n",
		                                DUMP_TITLE_FUNC,     def->func,
		                                                     def->line);
		kdbg_print(inst, "[%-8s] %s:%d\n",
		                                DUMP_TITLE_FILE,     def->file,
		                                                     def->line);
		kdbg_print(inst, "[%-8s] %s:%d\n", "IMPL",     def->impl->file,
		                                               def->impl->line);
	} else { // trace->type == KDBG_TRACE_IMP
		kdbg_trace_imp_t *imp = (kdbg_trace_imp_t *)trace;
		snprintf(ctx->namebuf, sizeof(ctx->namebuf), "%s:%s",
		    imp->mod, imp->name);
		kdbg_print(inst, "Details of Single-Trace [IMP]:\n");
		kdbg_print(inst, "------------------------------\n");
		kdbg_print(inst, "[%-7s] %d\n", DUMP_TITLE_TRACE_ID, trace->id);
		kdbg_print(inst, "[%-7s] %s\n", DUMP_TITLE_NAME,  ctx->namebuf);
		kdbg_print(inst, "[%-7s] %d\n", DUMP_TITLE_ARGC,     imp->argc);
		kdbg_print(inst, "[%-7s] %s:%d\n",
		                                "File",   imp->file, imp->line);
	}
}

static int
unregister_traces(trace_mgr_t *mgr, const char *mod)
{
	kdbg_trace_imp_t *imp_mod;
	if (!(imp_mod = imp_lookup(mgr, mod, TRACE_NAME_FOR_MOD, NULL, NULL)))
		return (0);

	int cnt = 0;
	kdbg_trace_imp_t *imp;
	for (imp = imp_mod; !!imp; imp = (kdbg_trace_imp_t *)imp->head.next)
		cnt += clear_all_traces(imp);

	if (imp_mod->hold_mod) {
		// TODO: release the reference of the module
		imp_mod->hold_mod = 0;
	}

	return (cnt);
}

static void
dump_ctx_init(dump_ctx_t *ctx, drv_inst_t *inst, trace_arr_t *arr)
{
	ctx->inst		= inst;
	ctx->trace_arr		= arr;
	ctx->sep		= "    ";
	ctx->invstr		= "--";
	ctx->stat_max_len	= 0;
	ctx->max_len		= 0;
	ctx->dump_cnt		= 0;
	ctx->namebuf[0]		= '\0';
	ctx->buffer[0]		= '\0';
	ctx->title[0]		= '\0';
	ctx->def_fmt[0]		= '\0';
	ctx->imp_fmt[0]		= '\0';
	memset(ctx->len, 0, sizeof(ctx->len));
}

static void
dump_stat_len(dump_ctx_t *ctx, int dump_ndx, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(ctx->buffer, sizeof(ctx->buffer), fmt, ap);
	va_end(ap);

	uint8_t str_size = strlen(ctx->buffer);
	ctx->len[dump_ndx] = MAX(str_size, ctx->len[dump_ndx]);
}

static inline const char *
fbase(const char *path)
{
	const char *base = strrchr(path, '/');
	return (base ? base + 1 : path);
}

static void
dump_stat_by_title(dump_ctx_t *ctx)
{
	dump_stat_len(ctx, DUMP_TRACE_ID, DUMP_TITLE_TRACE_ID);
	dump_stat_len(ctx, DUMP_NAME,     DUMP_TITLE_NAME);
	dump_stat_len(ctx, DUMP_ARGC,     DUMP_TITLE_ARGC);
	dump_stat_len(ctx, DUMP_ENABLE,   DUMP_TITLE_ENABLE);
	dump_stat_len(ctx, DUMP_FUNC,     DUMP_TITLE_FUNC);
	dump_stat_len(ctx, DUMP_LINE,     DUMP_TITLE_LINE);
	dump_stat_len(ctx, DUMP_FILE,     DUMP_TITLE_FILE);
}

static void
dump_stat_by_trace(dump_ctx_t *ctx, kdbg_trace_head_t *trace)
{
	if (trace->type == KDBG_TRACE_DEF) {
		kdbg_trace_def_t *def = (kdbg_trace_def_t *)trace;
		dump_stat_len(ctx, DUMP_TRACE_ID, "%d", def->head.id);
		dump_stat_len(ctx, DUMP_NAME,  "%s:%s", def->impl->mod,
		                                        def->name);
		dump_stat_len(ctx, DUMP_ARGC,     "%d", def->argc);
		dump_stat_len(ctx, DUMP_ENABLE,   "%s", bool2str(def->call));
		dump_stat_len(ctx, DUMP_FUNC,     "%s", def->func);
		dump_stat_len(ctx, DUMP_LINE,     "%d", def->line);
		dump_stat_len(ctx, DUMP_FILE,     "%s", fbase(def->file));
	} else { // trace->type == KDBG_TRACE_IMP
		kdbg_trace_imp_t *imp = (kdbg_trace_imp_t *)trace;
		dump_stat_len(ctx, DUMP_TRACE_ID, "%d", imp->head.id);
		dump_stat_len(ctx, DUMP_NAME,  "%s:%s", imp->mod, imp->name);
		dump_stat_len(ctx, DUMP_ARGC,     "%d", imp->argc);
		dump_stat_len(ctx, DUMP_ENABLE,   "%s", ctx->invstr);
		dump_stat_len(ctx, DUMP_FUNC,     "%s", ctx->invstr);
		dump_stat_len(ctx, DUMP_LINE,     "%s", ctx->invstr);
		dump_stat_len(ctx, DUMP_FILE,     "%s", ctx->invstr);
	}
}

static void
dump_stat_all_traces(trace_mgr_t *mgr, dump_ctx_t *ctx)
{
	dump_stat_by_title(ctx);
	for (int i = 0; i < ctx->trace_arr->arr_cnt; i++) {
		kdbg_trace_head_t *trace = ctx->trace_arr->arr[i];
		dump_stat_by_trace(ctx, trace);
	}
}

static void
dump_gen_format(dump_ctx_t *ctx)
{
	snprintf(ctx->def_fmt, sizeof(ctx->def_fmt),
	    "%%-%us%s%%-%us%s%%-%us%s%%-%us%s%%-%us%s%%-%us%s%%-%us\n",
	    ctx->len[DUMP_TRACE_ID], ctx->sep, ctx->len[DUMP_NAME], ctx->sep,
	    ctx->len[DUMP_ARGC], ctx->sep, ctx->len[DUMP_ENABLE], ctx->sep,
	    ctx->len[DUMP_FUNC], ctx->sep, ctx->len[DUMP_LINE], ctx->sep,
	    ctx->len[DUMP_FILE]);
	snprintf(ctx->title, sizeof(ctx->title), ctx->def_fmt,
	    DUMP_TITLE_TRACE_ID, DUMP_TITLE_NAME, DUMP_TITLE_ARGC,
	    DUMP_TITLE_ENABLE, DUMP_TITLE_FUNC, DUMP_TITLE_LINE,
	    DUMP_TITLE_FILE);

	snprintf(ctx->def_fmt, sizeof(ctx->def_fmt),
	    "%%-%ud%s%%-%us%s%%-%ud%s%%-%us%s%%-%us%s%%-%ud%s%%-%us\n",
	    ctx->len[DUMP_TRACE_ID], ctx->sep, ctx->len[DUMP_NAME], ctx->sep,
	    ctx->len[DUMP_ARGC], ctx->sep, ctx->len[DUMP_ENABLE], ctx->sep,
	    ctx->len[DUMP_FUNC], ctx->sep, ctx->len[DUMP_LINE], ctx->sep,
	    ctx->len[DUMP_FILE]);

	snprintf(ctx->imp_fmt, sizeof(ctx->imp_fmt),
	    "%%-%ud%s%%-%us%s%%-%ud%s%%-%us%s%%-%us%s%%-%us%s%%-%us\n",
	    ctx->len[DUMP_TRACE_ID], ctx->sep, ctx->len[DUMP_NAME], ctx->sep,
	    ctx->len[DUMP_ARGC], ctx->sep, ctx->len[DUMP_ENABLE], ctx->sep,
	    ctx->len[DUMP_FUNC], ctx->sep, ctx->len[DUMP_LINE], ctx->sep,
	    ctx->len[DUMP_FILE]);
}

static void
dump_sep_line(drv_inst_t *inst, int cnt)
{
	for (int i = cnt-1; i >= 0; i--)
		kdbg_print(inst, "%c", i==0 ? '\n' : '-');
}

static inline void
dump_trace_info_by_title(dump_ctx_t *ctx)
{
	if (ctx->stat_max_len)
		ctx->max_len = strlen(ctx->title);
	else {
		dump_sep_line(ctx->inst, ctx->max_len);
		kdbg_print(ctx->inst, "%s", ctx->title);
		dump_sep_line(ctx->inst, ctx->max_len);
	}
}

static void
dump_trace_info_by_trace(dump_ctx_t *ctx, kdbg_trace_head_t *trace)
{
#define DUMP_VAL_DEF							\
	def->head.id, ctx->namebuf, def->argc, bool2str(def->call),	\
	def->func, def->line, fbase(def->file)
#define DUMP_VAL_IMP							\
	imp->head.id, ctx->namebuf, imp->argc, ctx->invstr,		\
	ctx->invstr, ctx->invstr, ctx->invstr

	if (trace->type == KDBG_TRACE_DEF) {
		kdbg_trace_def_t *def = (kdbg_trace_def_t *)trace;
		snprintf(ctx->namebuf, sizeof(ctx->namebuf), "%s:%s",
		    def->impl->mod, def->name);
		if (!ctx->stat_max_len) {
			kdbg_print(ctx->inst, ctx->def_fmt,
			    DUMP_VAL_DEF);
		} else {
			snprintf(ctx->buffer, sizeof(ctx->buffer), ctx->def_fmt,
			    DUMP_VAL_DEF);
		}
	} else { // trace->type == KDBG_TRACE_IMP
		kdbg_trace_imp_t *imp = (kdbg_trace_imp_t *)trace;
		snprintf(ctx->namebuf, sizeof(ctx->namebuf), "%s:%s",
		    imp->mod, imp->name);
		if (!ctx->stat_max_len) {
			kdbg_print(ctx->inst, ctx->imp_fmt,
			     DUMP_VAL_IMP);
		} else {
			snprintf(ctx->buffer, sizeof(ctx->buffer), ctx->imp_fmt,
			     DUMP_VAL_IMP);
			ctx->max_len = MAX(ctx->max_len, strlen(ctx->buffer));
		}
	}

#undef DUMP_VAL_DEF
#undef DUMP_VAL_IMP
}

static void
dump_show_all_traces(trace_mgr_t *mgr, dump_ctx_t *ctx, int stat_max_len)
{
	ctx->stat_max_len = stat_max_len;
	dump_trace_info_by_title(ctx);
	for (int i = 0; i < ctx->trace_arr->arr_cnt; i++) {
		kdbg_trace_head_t *trace = ctx->trace_arr->arr[i];
		dump_trace_info_by_trace(ctx, trace);
	}
}

static void
dump_traces(trace_arr_t *arr)
{
	drv_inst_t *inst = arr->inst;
	trace_mgr_t *mgr = arr->mgr;

	dump_ctx_t *ctx = kmalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx) {
		kdbg_print(inst, "Out of memory\n");
		return;
	}
	dump_ctx_init(ctx, inst, arr);

	if (arr->arr_cnt == 1) {
		dump_single_trace(ctx, arr->arr[0]);
	} else if (arr->arr_cnt > 1) {
		dump_stat_all_traces(mgr, ctx);
		dump_gen_format(ctx);
		dump_show_all_traces(mgr, ctx, 1); // stat max len of all lines
		dump_show_all_traces(mgr, ctx, 0); // dump info
	} else {
		kdbg_print(inst, "No trace-points definitions.\n");
	}

	kfree(ctx);
}

void
kdbg_trace_init(void)
{
	trace_mgr_t *mgr = &trace_mgr;
	mgr_lock(mgr);
	trace_mgr_init(mgr);
	mgr_unlock(mgr);
}

int
kdbg_trace_register(const char *mod, kdbg_trace_def_t **defs)
{
	int changed = 0;

	trace_mgr_t *mgr = &trace_mgr;
	mgr_lock(mgr);
	changed |= !!unregister_traces(mgr, mod);

	register_stat_t r;
	register_traces(mgr, mod, defs, &r);
	changed |= r.ok_cnt > 0 || r.hang_cnt > 0;

	if (changed)
		trace_order(mgr);
	mgr_unlock(mgr);

	return (r.hang_cnt ? r.hang_cnt : -!!r.err_cnt);
}

#define TRACE_USAGE "<dump|enable|disable|cleanup> [traces]"
static void
trace_usage(drv_inst_t *inst, const char *cmd)
{
	const char *format[] = {
		"ALL",          "All of trace-points; No Traces given mean ALL",
		"TraceId",      "Such as '1 3 5 6 8 ...'",
		"TraceName",    "Such as 'kdbg:TP1 kdbg:TP2 zfs:zio_done ...'",
		"TracePattern", "Such as '^kdbg:TP ^kdbg:zio'",
		"TraceRange",   "Such as '1 . 4 6 . 9', that is '1~4,6~9'",
		NULL,           NULL
	};

	kdbg_print(inst, "Usage: %s %s\n", cmd, TRACE_USAGE);

	kdbg_print(inst, "\nThe format of 'traces':\n");
	dump_sep_line(inst, 62);
	for (const char **fmt = format; fmt[0] && fmt[1]; fmt += 2)
		kdbg_print(inst, "%-12s - %s\n", fmt[0], fmt[1]);
}

static int
str_to_cmd_no(const char *cmd)
{
	const char *cmd_tbl[TRACE_CMD_NUM] = {
		[TRACE_CMD_DUMP   ] = "dump",
		[TRACE_CMD_ENABLE ] = "enable",
		[TRACE_CMD_DISABLE] = "disable",
		[TRACE_CMD_CLEANUP] = "cleanup"
	};

	for (int i = 0; i < TRACE_CMD_NUM; i++) {
		if (cmd_tbl[i] && !strcmp(cmd_tbl[i], cmd))
			return (i);
	}

	return (-1);
}

static int
trace_parse_args(trace_args_t *args, drv_inst_t *inst, int argc, char *argv[])
{
	memset(args, 0, sizeof(*args));

	// argc=1: show usage
	if (argc == 1) {
		trace_usage(inst, argv[0]);
		return (-1);
	}

	// argc>=2: parse cmd
	args->cmd_no = str_to_cmd_no(argv[1]);
	if (args->cmd_no < 0) {
		kdbg_print(inst, "Error: *** invalid cmd(%s)\n", argv[1]);
		trace_usage(inst, argv[0]);
		return (-1);
	}

	// parse trace-id-type
	if (argc < 3 || !strcmp(argv[2], "ALL")) {
		args->traces_all = 1;
		return (0);
	}

	args->traces_cnt = argc - 2;
	args->traces_arr = &argv[2];
	args->traces_ndx = 0;
	return (0);
}

static int
trace_arr_init(trace_arr_t *arr, drv_inst_t *inst, trace_mgr_t *mgr)
{
	arr->mgr	 = mgr;
	arr->inst	 = inst;
	arr->arr	 = NULL;
	arr->arr_cnt	 = 0;
	arr->max_arr_cnt = 0;
	arr->err	 = 0;

	mgr_lock(mgr);
	arr->locked = 1;
	arr->is_empty = !mgr->trace_cnt;

	if (arr->is_empty) {
		arr->locked = 0;
		mgr_unlock(mgr);
		return (0);
	}

	arr->max_arr_cnt = mgr->trace_cnt;
	arr->arr = kmalloc(sizeof(arr->arr[0]) * arr->max_arr_cnt, GFP_KERNEL);
	if (!arr->arr) {
		arr->locked = 0;
		mgr_unlock(mgr);
		return (-1);
	}

	return (0);
}

static int
str2u64(const char *str, uint64_t *out)
{
	uint64_t r = 0;
	int cnt = 0;

	while (*str) {
		if (*str >= '0' && *str <= '9') {
			r = r * 10 + (*str - '0');
			cnt++;
			str++;
		} else {
			return (-1);
		}
	}

	if (out)
		*out = r;
	return (cnt > 0 ? 0 : -1);
}

static void
fill_single_trace(trace_arr_t *arr, kdbg_trace_head_t *trace)
{
	for (int i = 0; i < arr->arr_cnt; i++)
		if (arr->arr[i] == trace)
			return;

	if (arr->arr_cnt >= arr->max_arr_cnt) {
		kdbg_print(arr->inst, "Error: *** trace_array is full\n");
		arr->err = -1;
		return;
	}

	arr->arr[arr->arr_cnt++] = trace;
}

static int
fill_all(trace_arr_t *arr)
{
	trace_iter_t iter;
	kdbg_trace_head_t *trace;
	for_each_trace(arr->mgr, &iter, trace) {
		if (arr->arr_cnt >= arr->max_arr_cnt) {
			kdbg_print(arr->inst,
			    "Error: *** fill all but trace_array is full\n");
			arr->err = -1;
			break;
		}
		arr->arr[arr->arr_cnt++] = trace;
	}

	return (arr->err);
}

static void
fill_trace_by_id(trace_arr_t *arr, uint64_t id)
{
	trace_iter_t iter;
	kdbg_trace_head_t *trace;
	for_each_trace(arr->mgr, &iter, trace) {
		if (trace->id == id) {
			fill_single_trace(arr, trace);
			return;
		}
	}

	kdbg_print(arr->inst,
	    "Error: *** Failed to lookup trace by id(%llu)\n", id);
	arr->err = -1;
}

static void
gen_trace_name(kdbg_trace_head_t *trace, char *buf, size_t size)
{
	if (trace->type == KDBG_TRACE_DEF) {
		kdbg_trace_def_t *def = (kdbg_trace_def_t *)trace;
		snprintf(buf, size, "%s:%s", def->impl->mod, def->name);
	} else { // trace->type == KDBG_TRACE_IMP
		kdbg_trace_imp_t *imp = (kdbg_trace_imp_t *)trace;
		snprintf(buf, size, "%s:%s", imp->mod, imp->name);
	}
}

static void
fill_trace_by_name(trace_arr_t *arr, const char *name, int exact)
{
	trace_iter_t iter;
	kdbg_trace_head_t *trace;

	int len = 0, match;
	if (!exact && !(len = strlen(name))) {
		kdbg_print(arr->inst, "Error: *** pattern(^) is empty\n");
		arr->err = -1;
		return;
	}

	char namebuf[128];
	for_each_trace(arr->mgr, &iter, trace) {
		gen_trace_name(trace, namebuf, sizeof(namebuf));
		match = exact ? !strcmp(namebuf, name) :
		    !strncmp(namebuf, name, len);
		if (match) {
			fill_single_trace(arr, trace);
			if (arr->err)
				break;
		}
	}
}

static void
fill_trace_by_range(trace_arr_t *arr, const char *arg)
{
	if (*arg == '^') {
		kdbg_print(arr->inst,
		    "Error: *** TracePattern is not supported in TraceRange\n");
		arr->err = -1;
		return;
	}

	uint64_t id = 0;
	if (str2u64(arg, &id))
		id = 0;

	kdbg_trace_head_t *start = NULL;
	if (arr->arr_cnt > 0)
		start = arr->arr[arr->arr_cnt - 1];

	char namebuf[128];
	trace_iter_t iter;
	kdbg_trace_head_t *trace, *end = NULL;
	for_each_trace(arr->mgr, &iter, trace) {
		if (!start)
			start = trace;

		if (trace->id == id) {
			end = trace;
			break;
		}

		gen_trace_name(trace, namebuf, sizeof(namebuf));
		if (!strcmp(namebuf, arg)) {
			end = trace;
			break;
		}
	}

	if (!end) {
		kdbg_print(arr->inst, "Error: *** Failed to lookup trace(%s)\n",
		    arg);
		arr->err = -1;
		return;
	}

	if (start->id > end->id)
		return;

	for_each_trace(arr->mgr, &iter, trace) {
		if (trace->id < start->id)
			continue;
		else if (trace->id > end->id)
			break;
		else
			fill_single_trace(arr, trace);
	}
}

static int
trace_arr_fill(trace_arr_t *arr, trace_args_t *args)
{
	if (args->traces_all)
		return (fill_all(arr));

	uint64_t r;
	int is_range = 0;
	for (int i = 0; !arr->err && i < args->traces_cnt; i++) {
		const char *arg = args->traces_arr[i];
		if (is_range) {
			is_range = 0;
			fill_trace_by_range(arr, arg);
		} else if (!str2u64(arg, &r)) {
			fill_trace_by_id(arr, r);
		} else if (*arg == '^') {
			fill_trace_by_name(arr, &arg[1], 0);
		} else if (!strcmp(arg, ".")) {
			is_range = 1;
		} else {
			fill_trace_by_name(arr, arg, 1);
		}
	}

	return (arr->err);
}

static void
trace_arr_destroy(trace_arr_t *arr)
{
	if (arr->locked) {
		arr->locked = 0;
		mgr_unlock(arr->mgr);
	}

	if (arr->arr) {
		kfree(arr->arr);
		arr->arr = NULL;
	}
}

static void
do_trace_switch(trace_arr_t *arr, int enable)
{
	const char *status[] = { "disable", "enable" };
	const char *op = status[!!enable];

	if (arr->arr_cnt == 0) {
		kdbg_print(arr->inst, "No definitions found to %s\n", op);
		return;
	}

	int repeat, ok, err;
	repeat = ok = err = 0;

	for (int i = 0; i < arr->arr_cnt; i++) {
		kdbg_trace_head_t *trace = arr->arr[i];
		if (trace->type == KDBG_TRACE_IMP) {
			err++;
			continue;
		}

		kdbg_trace_def_t *def = (kdbg_trace_def_t*)trace;
		if (!!def->call != !!enable) {
			if (enable)
				enable_trace(def);
			else
				disable_trace(def);
			ok++;
		} else {
			repeat++;
		}
	}

	kdbg_print(arr->inst, "Info: %s %d traces\n", op, ok);
}

static void
cmd_enable_trace(trace_arr_t *arr)
{
	do_trace_switch(arr, 1);
}

static void
cmd_disable_trace(trace_arr_t *arr)
{
	do_trace_switch(arr, 0);
}

static void
exec_cmd(trace_args_t *args, trace_arr_t *arr)
{
	switch (args->cmd_no) {
	case TRACE_CMD_DUMP:
		dump_traces(arr);
		break;

	case TRACE_CMD_ENABLE:
		cmd_enable_trace(arr);
		break;

	case TRACE_CMD_DISABLE:
		cmd_disable_trace(arr);
		break;

	case TRACE_CMD_CLEANUP:
		kdbg_print(arr->inst, "Error: *** cleanup not implement\n");
		break;

	default:
		kdbg_print(arr->inst, "Error: *** invalid cmdNo(%d)\n",
		    args->cmd_no);
		break;
	}
}

KDBG_CMD_DEF(trace, TRACE_USAGE, drv_inst_t *inst, int argc, char *argv[])
{
	trace_mgr_t *mgr = &trace_mgr;

	trace_args_t args;
	if (trace_parse_args(&args, inst, argc, argv))
		return (0);

	trace_arr_t arr;
	if (trace_arr_init(&arr, inst, mgr)) {
		kdbg_print(inst, "Error: Out of memory\n");
		return (0);
	}

	if (!trace_arr_fill(&arr, &args))
		exec_cmd(&args, &arr);

	trace_arr_destroy(&arr);
	return (0);
}
