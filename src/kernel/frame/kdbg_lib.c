#include <sys/kdbg.h>
#include <sys/kdbg_impl.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>

static inline void
lbi_init(lbi_t *lbi)
{
	lbi->next = NULL;
	lbi->rd = lbi->wr = 0;
	lbi->size = LOG_BUF_SIZE - 1;
	lbi->buf[0] = lbi->buf[lbi->size] = '\0';
}

static lbi_t *
lbi_alloc(void)
{
	lbi_t *lbi = kmalloc(LOG_BUF_STRUCT_SIZE, GFP_KERNEL);
	if (lbi)
		lbi_init(lbi);
	return (lbi);
}

static void
lbi_free(lbi_t *lbi)
{
	while (lbi) {
		lbi_t *next = lbi->next;
		kfree(lbi);
		lbi = next;
	}
}

static inline int
lbi_writable(lbi_t *lbi)
{
	return (lbi && lbi->size > 0);
}

static inline int
lbi_readable(lbi_t *lbi)
{
	return (lbi && lbi->rd < lbi->wr);
}

static void
lbi_write(lbi_t *lbi, char **pbuf, int *size)
{
	if (lbi_writable(lbi) && *size > 0) {
		int wrsz = MIN(*size, lbi->size);
		memcpy(&lbi->buf[lbi->wr], *pbuf, wrsz);

		lbi->wr += wrsz;
		lbi->size -= wrsz;

		*pbuf += wrsz;
		*size -= wrsz;
	}
}

static int
lbi_uread(lbi_t *lbi, char **pbuf, int *size)
{
	if (lbi_readable(lbi) && *size > 0) {
		int avail = lbi->wr - lbi->rd;
		int rdsz = MIN(*size, avail);
		if (kdbg_copyto(*pbuf, &lbi->buf[lbi->rd], rdsz))
			return (-EFAULT);

		lbi->rd += rdsz;
		*pbuf += rdsz;
		*size -= rdsz;
	}

	return (0);
}

static inline void
lbl_init(lbl_t *list)
{
	list->head = list->tail = NULL;
	list->cnt = 0;
}

static void
lbl_destroy(lbl_t *list)
{
	lbi_t *lbi = list->head;
	lbl_init(list);
	lbi_free(lbi);
}

static void
lbl_push(lbl_t *list, lbi_t *lbi)
{
	if (list->tail) {
		list->tail->next = lbi;
		list->tail = lbi;
	} else {
		list->head = list->tail = lbi;
	}
	list->cnt++;
	lbi->next = NULL;
}

static lbi_t *
lbl_pop(lbl_t *list)
{
	lbi_t *lbi = list->head;
	if (list->cnt > 0) {
		list->cnt--;
		list->head = lbi->next;
		if (list->cnt == 0)
			list->tail = NULL;
		lbi->next = NULL;
	}
	return (lbi);
}

static unsigned int
logbuf_expand(logbuf_t *lb)
{
	unsigned int orig_cnt = lb->free.cnt;

	while (lb->free.cnt < MAX_FREE_LOG_BUF_CNT) {
		lbi_t *lbi = lbi_alloc();
		if (lbi)
			lbl_push(&lb->free, lbi);
		else
			break;
	}

	return (lb->free.cnt - orig_cnt);
}

static lbi_t *
logbuf_write_get(logbuf_t *lb)
{
	lbi_t *lbi = lb->busy.tail;
	if (lbi_writable(lbi))
		return (lbi);

	if (!lb->free.cnt && !logbuf_expand(lb))
		return (NULL);

	lbi = lbl_pop(&lb->free);
	lbl_push(&lb->busy, lbi);

	return (lbi);
}

static lbi_t *
logbuf_read_get(logbuf_t *lb)
{
	while (lb->busy.cnt > 0) {
		lbi_t *lbi = lb->busy.head;
		BUG_ON(!lbi);

		if (lbi_readable(lbi))
			return (lbi);
		else {
			lbi = lbl_pop(&lb->busy);
			if (lb->free.cnt < MAX_FREE_LOG_BUF_CNT) {
				lbi_init(lbi);
				lbl_push(&lb->free, lbi);
			} else
				lbi_free(lbi);
		}
	}

	return (NULL);
}

static int
logbuf_init(logbuf_t *lb)
{
	lbl_init(&lb->free);
	lbl_init(&lb->busy);
	return (logbuf_expand(lb) > 0 ? 0 : -1);
}

static void
logbuf_destroy(logbuf_t *lb)
{
	lbl_destroy(&lb->free);
	lbl_destroy(&lb->busy);
}

static int
logbuf_write(logbuf_t *lb, char *buf, int size)
{
	int remain = size;
	char *pbuf = buf;

	while (remain > 0) {
		lbi_t *lbi = logbuf_write_get(lb);
		if (lbi_writable(lbi))
			lbi_write(lbi, &pbuf, &remain);
		else
			break;
	}

	return (size - remain);
}

static int
logbuf_uread(logbuf_t *lb, char __user *buf, int size)
{
	int remain = size, rc;
	char *pbuf = buf;

	while (remain > 0) {
		lbi_t *lbi = logbuf_read_get(lb);
		if (lbi_readable(lbi)) {
			if ((rc = lbi_uread(lbi, &pbuf, &remain)) < 0)
				return (rc);
		} else
			break;
	}

	return (size - remain);
}

static inline void
usrcmd_init(usrcmd_t *uc)
{
	uc->cmdline = NULL;
	uc->cmdsz = 0;
	uc->argc = 0;
}

static inline void
usrcmd_destroy(usrcmd_t *uc)
{
	if (uc->cmdline)
		kfree(uc->cmdline);
	usrcmd_init(uc);
}

static char *
str_skip(char *str, char ch)
{
	while (*str && *str == ch)
		str++;
	return (*str ? str : NULL);
}

static char *
str_reach(char *str, char ch)
{
	while (*str && *str != ch)
		str++;
	return (*str ? str : NULL);
}

static int
usrcmd_split(drv_inst_t *inst, usrcmd_t *uc)
{
	char *str = str_skip(uc->cmdline, '\n');
	if (!str)
		return (0);
	uc->argv[uc->argc++] = str;

	for (;;) {
		if (!(str = str_reach(str, '\n')))
			break;
		*str++ = '\0';

		if (!(str = str_skip(str, '\n')))
			break;
		if (uc->argc >= MAX_USR_CMD_ARGC) {
			kdbg_print(inst, "Number of args are more than %d,",
			    MAX_USR_CMD_ARGC);
			for (int i = 0; i < uc->argc; i++)
				kdbg_print(inst, " %s", uc->argv[i]);
			kdbg_print(inst, " ...\n");
			return (-1);
		}

		uc->argv[uc->argc++] = str;
	}

	return (0);
}

static void
usrcmd_move(usrcmd_t *dst, usrcmd_t *src)
{
	usrcmd_destroy(dst);

	dst->cmdline = src->cmdline;
	dst->cmdsz = src->cmdsz;

	dst->argc = src->argc;
	for (int i = 0; i < src->argc; i++)
		dst->argv[i] = src->argv[i];

	usrcmd_init(src);
}

int
drv_inst_set_usrcmd(drv_inst_t *inst, const char __user *buf, size_t size)
{
	int rc = 0;

	usrcmd_t uc;
	usrcmd_init(&uc);

	uc.cmdline = kmalloc(size + 1, GFP_KERNEL);
	if (!uc.cmdline) {
		rc = -ENOMEM;
		goto done;
	}

	uc.cmdsz = size + 1;
	uc.cmdline[size] = '\0';

	rc = kdbg_copyfrom(uc.cmdline, buf, size);
	if (rc) {
		rc = -EFAULT;
		goto done;
	}

	rc = usrcmd_split(inst, &uc);
	if (rc) {
		rc = -EINVAL;
		goto done;
	}

	mutex_lock(&inst->mtx);
	usrcmd_move(&inst->usrcmd, &uc);
	mutex_unlock(&inst->mtx);

done:
	usrcmd_destroy(&uc);
	return (rc);
}

static inline int
drv_inst_init(drv_inst_t *inst, struct file *file)
{
	mutex_init(&inst->mtx);
	inst->file = file;
	usrcmd_init(&inst->usrcmd);
	return (logbuf_init(&inst->logbuf));
}

drv_inst_t *
drv_inst_alloc(struct file *file)
{
	drv_inst_t *inst = kmalloc(sizeof(*inst), GFP_KERNEL);
	if (!inst)
		return (NULL);

	if (drv_inst_init(inst, file)) {
		kfree(inst);
		return (NULL);
	}

	return (inst);
}

void
drv_inst_free(drv_inst_t *inst)
{
	if (inst) {
		logbuf_destroy(&inst->logbuf);
		usrcmd_destroy(&inst->usrcmd);
		kfree(inst);
	}
}

void
kdbg_vlog(const char *fmt, va_list ap)
{
	char buffer[1024];
	vsnprintf(buffer, sizeof(buffer), fmt, ap);
	printk(KERN_INFO "%s", buffer);
}

void
_kdbg_log(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	kdbg_vlog(fmt, ap);
	va_end(ap);
}

int
kdbg_read_screen(drv_inst_t *_inst, char __user *buf, size_t len)
{
	int rc = -1;
	drv_inst_t *inst = drv_inst(_inst);
	if (inst) {
		mutex_lock(&inst->mtx);
		rc = logbuf_uread(&inst->logbuf, buf, len);
		mutex_unlock(&inst->mtx);
	}
	return (rc);
}

void
kdbg_vprint(drv_inst_t *_inst, const char *fmt, va_list ap)
{
	char buffer[1024];
	vsnprintf(buffer, sizeof(buffer), fmt, ap);

	drv_inst_t *inst = drv_inst(_inst);
	if (inst) {
		int size = strlen(buffer);
		mutex_lock(&inst->mtx);
		int wrsz = logbuf_write(&inst->logbuf, buffer, size);
		mutex_unlock(&inst->mtx);
		if (size == wrsz)
			return;
	}

	printk(KERN_INFO "KDBG-SCREEN: %s\n", buffer);
}

void
_kdbg_print(drv_inst_t *inst, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	kdbg_vprint(inst, fmt, ap);
	va_end(ap);
}

int
kdbg_copyto(void *to, const void *from, unsigned long size)
{
	return (copy_to_user(to, from, size));
}

int
kdbg_copyfrom(void *to, const void *from, unsigned long size)
{
	return (copy_from_user(to, from, size));
}
