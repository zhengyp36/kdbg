#ifndef _SYS_KDBG_IMPL_H
#define _SYS_KDBG_IMPL_H

#include <stdarg.h>
#include <linux/mutex.h>

#ifndef MIN
#define MIN(x,y) ((x) < (y) ? (x) : (y))
#endif

#ifndef offsetof
#define offsetof(s,m) ((size_t)&((s*)0)->m)
#endif

#define MAX_FREE_LOG_BUF_CNT	16
#define LOG_BUF_STRUCT_SIZE	4096
#define LOG_BUF_HEAD_SIZE	offsetof(lbi_t,buf)
#define LOG_BUF_SIZE		(LOG_BUF_STRUCT_SIZE - LOG_BUF_HEAD_SIZE)

typedef struct log_buf_impl {
	struct log_buf_impl *	next;
	unsigned int		rd;
	unsigned int		wr;
	unsigned int		size;
	char			buf[0];
} lbi_t;

typedef struct log_buf_list {
	lbi_t *		head;
	lbi_t *		tail;
	unsigned int	cnt;
} lbl_t;

typedef struct log_buf {
	lbl_t	free;
	lbl_t	busy;
} logbuf_t;

#define MAX_USR_CMD_ARGC 32
typedef struct usrcmd {
	char *	cmdline;
	char *	argv[MAX_USR_CMD_ARGC];
	int	cmdsz;
	int	argc;
} usrcmd_t;

struct file;

typedef struct drv_inst {
	struct mutex		mtx;
	struct file *		file;
	logbuf_t		logbuf;
	usrcmd_t		usrcmd;
} drv_inst_t;

drv_inst_t *drv_inst_alloc(struct file *);
void drv_inst_free(drv_inst_t *);
drv_inst_t *drv_inst(drv_inst_t *);

void kdbg_vlog(const char *, va_list)
    __attribute__((format(printf,1,0)));

void kdbg_vprint(drv_inst_t *, const char *, va_list)
    __attribute__((format(printf,2,0)));

int kdbg_read_screen(drv_inst_t *, char __user *, size_t);

#define KDBG_SEC(sec) __attribute__((section(sec)))

typedef struct kdbg_cmd_impl {
	const char *name;
	const char *help;
	int (*entry)(drv_inst_t *, int argc, char *argv[]);
} cmd_impl_t;

#define KDBG_CMD_DEF(_name,_help,_arg0,_arg1,_arg2)			\
static int __kdbg_cmd_entry_##_name(drv_inst_t *, int, char *[]);	\
									\
static cmd_impl_t __kdbg_cmd_ctrl_##_name = {				\
	.name  = #_name,						\
	.help  = _help,							\
	.entry = __kdbg_cmd_entry_##_name				\
};									\
									\
KDBG_SEC("._kdbg.cmd") cmd_impl_t * __kdbg_cmd_ctrl_ptr_##_name =	\
	&__kdbg_cmd_ctrl_##_name;					\
									\
static int __kdbg_cmd_entry_##_name(_arg0, _arg1, _arg2)

#define KDBG_CMD_DEF_START()	\
	KDBG_SEC("._kdbg.cmd") cmd_impl_t *__kdbg_start_cmd
	extern cmd_impl_t *__kdbg_start_cmd;
#define KDBG_CMD_DEF_STOP()	\
	KDBG_SEC("._kdbg.cmd") cmd_impl_t *__kdbg_stop_cmd
	extern cmd_impl_t *__kdbg_stop_cmd;

int drv_inst_set_usrcmd(drv_inst_t *, const char __user *, size_t);
int kdbg_main(drv_inst_t *);

#endif // _SYS_KDBG_IMPL_H
