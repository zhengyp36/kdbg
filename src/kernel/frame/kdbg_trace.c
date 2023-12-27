#include <stdarg.h>
#include <sys/kdbg.h>
#include <sys/kdbg_impl.h>
#include <sys/kdbg_def_cmd.h>
#include <sys/kdbg_trace_impl.h>
#include <linux/slab.h>
#include <linux/kernel.h>

static kdbg_trace_imp_t *trace_head;

static inline void
def_list_init(kdbg_trace_def_list_t *list)
{
	list->head = NULL;
	list->cnt = 0;
}

static void
def_list_add(kdbg_trace_def_list_t *list,
    kdbg_trace_def_t *def, kdbg_trace_imp_t *imp)
{
	kdbg_trace_def_t **iter;
	for (iter = &list->head; !!*iter; iter = &(*iter)->next)
		if (*iter == def)
			return;

	list->cnt++;
	def->impl = imp;
	*iter = def;
}

static inline void
impl_clear_defs(kdbg_trace_imp_t *imp)
{
	def_list_init(&imp->defs);
	def_list_init(&imp->defs_hang);
}

static int
impl_disable_defs(kdbg_trace_imp_t *imp)
{
	int cnt = 0;

	kdbg_trace_def_t *def;
	for (def = imp->defs.head; !!def; def = def->next) {
		cnt += !!def->enable;
		def->enable = 0;
	}

	return (cnt);
}

static inline kdbg_trace_imp_t *
impl_init(kdbg_trace_imp_t *imp, kdbg_trace_imp_t *curr_mod)
{
	impl_clear_defs(imp);
	imp->next = imp->next_mod = NULL;
	imp->curr_mod = curr_mod ? curr_mod : imp;
	return (imp);
}

/* Note that where_mod MUST be initialized */
static kdbg_trace_imp_t *
impl_lookup(const char *mod, const char *name,
    kdbg_trace_imp_t ***where, kdbg_trace_imp_t ***where_mod)
{
	kdbg_trace_imp_t **iter = &trace_head;
	if (!name)
		name = "_";

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
	while (*iter && strcmp((*iter)->name, name))
		iter = &(*iter)->next;
	if (where)
		*where = iter;

	return (*iter);
}

static int update_trace_numbers(void);

void
kdbg_trace_init(void)
{
	kdbg_trace_imp_t *imp, **where, **where_mod = NULL;
	kdbg_for_each_trace_impl(imp) {
		if (!impl_lookup(imp->mod, imp->name, &where, &where_mod))
			*where = impl_init(imp, *where_mod);
	}

	kdbg_log("There are %d definitions.", update_trace_numbers());
}

int
kdbg_trace_register(const char *mod, kdbg_trace_def_t **defs)
{
	kdbg_trace_def_t **pdef, *def;
	kdbg_trace_imp_t *imp, **where, **where_mod = NULL;
	int errcnt = 0;

	for (pdef = defs; !!(def = *pdef); pdef++) {
		if (!!(imp = impl_lookup(mod, def->name, &where, &where_mod))) {
			def_list_add(&imp->defs, def, imp);
			kdbg_log("Register trace-def: "
			    "name=%s,call=0x%lx,argc=%d,traceid=%d,line=%d,"
			    "func=%s,file=%s",
			    def->name, (unsigned long)def->call, def->argc,
			    def->traceid, def->line, def->func, def->file);
		} else {
			kdbg_log("Error: *** No trace-point(%s:%s) found, "
			    "errcnt=%d", mod, def->name, ++errcnt);
			if (where_mod && *where_mod) {
				imp = *where_mod;
				def_list_add(&imp->defs_hang, def, imp);
			}
		}
	}

	return (errcnt);
}

static kdbg_trace_imp_t *
impl_next(kdbg_trace_imp_t *imp)
{
	if (!imp)
		return (NULL);
	else if (imp->next)
		return (imp->next);
	else if (!imp->curr_mod)
		return (NULL);
	else
		return (imp->curr_mod->next_mod);
}

int
kdbg_trace_unregister_all(void)
{
	int cnt = 0;

	for (kdbg_trace_imp_t *imp = trace_head; !!imp; imp = impl_next(imp)) {
		cnt += impl_disable_defs(imp);
		impl_clear_defs(imp);
	}

	return (cnt);
}

typedef struct visitor {
	int (*visit_imp)(struct visitor *, kdbg_trace_imp_t *, int is_enter);
	int (*visit_def)(struct visitor *, kdbg_trace_def_t *, int is_hang);
	void * ctx;
} visitor_t;

static int
traverse_traces(visitor_t *visitor)
{
	kdbg_trace_imp_t *imp;
	kdbg_trace_def_t *def;

	int rc = 0;
	for (imp = trace_head; !rc && !!imp; imp = impl_next(imp)) {
		rc = visitor->visit_imp(visitor, imp, 1);
		for (def = imp->defs.head; !rc && !!def; def = def->next)
			rc = visitor->visit_def(visitor, def, 0);
		for (def = imp->defs_hang.head; !rc && !!def; def = def->next)
			rc = visitor->visit_def(visitor, def, 1);
		if (!rc)
			rc = visitor->visit_imp(visitor, imp, 0);
	}

	return (rc);
}

static int
order_trace_def(visitor_t *visitor, kdbg_trace_def_t *def, int is_hang)
{
	def->traceid = ++*(int*)(visitor->ctx);
	return (0);
}

static int
order_visit_imp(visitor_t *visitor, kdbg_trace_imp_t *imp, int is_enter)
{
	if (is_enter)
		imp->traceid = *(int*)(visitor->ctx) + 1;
	else if (imp->defs.cnt + imp->defs_hang.cnt == 0 &&
	    strcmp(imp->name, KDBG_TRACE_INVALID_NAME) != 0)
		imp->traceid = ++*(int*)(visitor->ctx);
	return (0);
}

static int
update_trace_numbers(void)
{
	int defcnt = 0;
	visitor_t visitor = {
		.visit_def = order_trace_def,
		.visit_imp = order_visit_imp,
		.ctx = &defcnt
	};
	traverse_traces(&visitor);
	return (defcnt);
}

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

#define DUMP_BUF_LENGTH 1024
#define DUMP_NAME_BUF_LEN 128
typedef struct dump_ctx {
	drv_inst_t *	inst;
	const char *	sep;
	const char *	invstr;
	int		defcnt;
	int		stat_max_len;
	int		max_len;
	uint8_t		len[DUMP_MAX_ENTRY];
	char		def_fmt[DUMP_BUF_LENGTH];
	char		imp_fmt[DUMP_BUF_LENGTH];
	char		title[DUMP_BUF_LENGTH];
	char		namebuf[DUMP_NAME_BUF_LEN];
	char		buffer[DUMP_BUF_LENGTH];
} dump_ctx_t;

static void
dump_ctx_init(dump_ctx_t *ctx, drv_inst_t *inst)
{
	ctx->inst		= inst;
	ctx->sep		= "    ";
	ctx->invstr		= "--";
	ctx->defcnt		= 0;
	ctx->stat_max_len	= 0;
	ctx->max_len		= 0;
	ctx->def_fmt[0]		= '\0';
	ctx->imp_fmt[0]		= '\0';
	ctx->title[0]		= '\0';
	ctx->buffer[0]		= '\0';
	memset(ctx->len, 0, sizeof(ctx->len));
}

#ifndef MAX
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#endif

__attribute__((format(printf,3,4)))
static void dump_stat_len(dump_ctx_t *, int, const char *, ...);

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

static void
dump_stat_len_by_title(dump_ctx_t *ctx)
{
	dump_stat_len(ctx, DUMP_TRACE_ID, DUMP_TITLE_TRACE_ID);
	dump_stat_len(ctx, DUMP_NAME,     DUMP_TITLE_NAME);
	dump_stat_len(ctx, DUMP_ARGC,     DUMP_TITLE_ARGC);
	dump_stat_len(ctx, DUMP_ENABLE,   DUMP_TITLE_ENABLE);
	dump_stat_len(ctx, DUMP_FUNC,     DUMP_TITLE_FUNC);
	dump_stat_len(ctx, DUMP_LINE,     DUMP_TITLE_LINE);
	dump_stat_len(ctx, DUMP_FILE,     DUMP_TITLE_FILE);
}

static const char *
fbase(const char *path)
{
	const char *base = strrchr(path, '/');
	return (base ? base + 1 : path);
}

static int
dump_stat_len_by_def(visitor_t *visitor, kdbg_trace_def_t *def, int is_hang)
{
	dump_ctx_t *ctx = visitor->ctx;
	dump_stat_len(ctx, DUMP_TRACE_ID, "%d", def->traceid);
	dump_stat_len(ctx, DUMP_NAME,  "%s:%s", def->impl->mod, def->name);
	dump_stat_len(ctx, DUMP_ARGC,     "%d", def->argc);
	dump_stat_len(ctx, DUMP_ENABLE,   "%s", def->enable ? "true" : "false");
	dump_stat_len(ctx, DUMP_FUNC,     "%s", def->func);
	dump_stat_len(ctx, DUMP_LINE,     "%d", def->line);
	dump_stat_len(ctx, DUMP_FILE,     "%s", fbase(def->file));
	ctx->defcnt++;
	return (0);
}

static int
dump_stat_len_by_imp(visitor_t *visitor, kdbg_trace_imp_t *imp, int is_enter)
{
	if (!is_enter && (imp->defs.cnt + imp->defs_hang.cnt == 0) &&
	    strcmp(imp->name, KDBG_TRACE_INVALID_NAME) != 0) {
		dump_ctx_t *ctx = visitor->ctx;
		dump_stat_len(ctx, DUMP_TRACE_ID, "%d", imp->traceid);
		dump_stat_len(ctx, DUMP_NAME,  "%s:%s", imp->mod, imp->name);
		dump_stat_len(ctx, DUMP_ARGC,     "%d", imp->argc);
		dump_stat_len(ctx, DUMP_ENABLE,   "%s", ctx->invstr);
		dump_stat_len(ctx, DUMP_FUNC,     "%s", ctx->invstr);
		dump_stat_len(ctx, DUMP_LINE,     "%s", ctx->invstr);
		dump_stat_len(ctx, DUMP_FILE,     "%s", ctx->invstr);
		ctx->defcnt++;
	}
	return (0);
}

static int
dump_trace_info_by_def(visitor_t *visitor, kdbg_trace_def_t *def, int is_hang)
{
	dump_ctx_t *ctx = visitor->ctx;
	snprintf(ctx->namebuf, sizeof(ctx->namebuf), "%s:%s",
	    def->impl->mod, def->name);

	if (!ctx->stat_max_len)
		kdbg_print(ctx->inst, ctx->def_fmt, def->traceid, ctx->namebuf,
		    def->argc, def->enable ? "true" : "false", def->func,
		    def->line, fbase(def->file));
	else {
		snprintf(ctx->buffer, sizeof(ctx->buffer), ctx->def_fmt,
		    def->traceid, ctx->namebuf, def->argc,
		    def->enable ? "true" : "false", def->func, def->line,
		    fbase(def->file));
		ctx->max_len = MAX(ctx->max_len, strlen(ctx->buffer));
	}

	return (0);
}

static int
dump_trace_info_by_imp(visitor_t *visitor, kdbg_trace_imp_t *imp, int is_enter)
{
	if (!is_enter && (imp->defs.cnt + imp->defs_hang.cnt == 0) &&
	    strcmp(imp->name, KDBG_TRACE_INVALID_NAME) != 0) {
		dump_ctx_t *ctx = visitor->ctx;
		snprintf(ctx->namebuf, sizeof(ctx->namebuf), "%s:%s",
		    imp->mod, imp->name);
		if (!ctx->stat_max_len)
			kdbg_print(ctx->inst, ctx->imp_fmt,
			     imp->traceid, ctx->namebuf, imp->argc, ctx->invstr,
			     ctx->invstr, ctx->invstr, ctx->invstr);
		else {
			snprintf(ctx->buffer, sizeof(ctx->buffer), ctx->imp_fmt,
			     imp->traceid, ctx->namebuf, imp->argc, ctx->invstr,
			     ctx->invstr, ctx->invstr, ctx->invstr);
			ctx->max_len = MAX(ctx->max_len, strlen(ctx->buffer));
		}
	}
	return (0);
}

static void
do_stat_dump_len(dump_ctx_t *ctx)
{
	visitor_t visitor = {
		.visit_def = dump_stat_len_by_def,
		.visit_imp = dump_stat_len_by_imp,
		.ctx = ctx
	};

	dump_stat_len_by_title(ctx);
	traverse_traces(&visitor);
}

static void
dump_sep_line(dump_ctx_t *ctx)
{
	for (int i = ctx->max_len-1; i >= 0; i--)
		kdbg_print(ctx->inst, "%c", i==0 ? '\n' : '-');
}

static void
do_dump_traces(dump_ctx_t *ctx, int stat_max_len)
{
	visitor_t visitor = {
		.visit_def = dump_trace_info_by_def,
		.visit_imp = dump_trace_info_by_imp,
		.ctx = ctx
	};

	ctx->stat_max_len = stat_max_len;
	if (ctx->stat_max_len) {
		ctx->max_len = strlen(ctx->title);
	} else {
		dump_sep_line(ctx);
		kdbg_print(ctx->inst, "%s", ctx->title);
		dump_sep_line(ctx);
	}
	traverse_traces(&visitor);

	if (!ctx->stat_max_len)
		dump_sep_line(ctx);
}

static void
gen_dump_format(dump_ctx_t *ctx)
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
dump_traces(drv_inst_t *inst)
{
	dump_ctx_t *ctx = kmalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx) {
		kdbg_print(inst, "Out of memory\n");
		return;
	}
	dump_ctx_init(ctx, inst);

	do_stat_dump_len(ctx);
	if (ctx->defcnt > 0){
		gen_dump_format(ctx);
		do_dump_traces(ctx, 1);
		do_dump_traces(ctx, 0);
	} else {
		kdbg_print(inst, "No trace-points definitions.\n");
	}

	kfree(ctx);
}

void
kdbg_trace_update_order(void)
{
	kdbg_log("Update traces numbers: %d.", update_trace_numbers());
}

KDBG_CMD_DEF(trace, "", drv_inst_t *inst, int argc, char *argv[])
{
	dump_traces(inst);
	return (0);
}
