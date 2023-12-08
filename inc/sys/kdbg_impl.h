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

int drv_inst_set_usrcmd(drv_inst_t *, const char __user *, size_t);
int kdbg_main(drv_inst_t *);

#endif // _SYS_KDBG_IMPL_H
