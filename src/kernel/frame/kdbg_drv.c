#include <sys/ksym.h>
#include <sys/kdbg.h>
#include <sys/kdbg_impl.h>
#include <sys/kdbg_trace_impl.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>

enum {
	STATE_ALLOC_CDEV   = 1 << 0,
	STATE_ADD_CDEV     = 1 << 1,
	STATE_CREATE_CLASS = 1 << 2,
	STATE_CREATE_DEV   = 1 << 3,
};

typedef int (uevent_pfn_t)(struct device *, struct kobj_uevent_env *);

typedef struct drv_ctrl {
	unsigned int		state;
	struct file_operations	fops;
	dev_t			dev;
	struct cdev		cdev;
	struct class *		dev_class;
	struct device *		device;
	uevent_pfn_t *		uevent;
} drv_ctrl_t;

static drv_inst_t *default_drv_inst;

drv_inst_t *
drv_inst(drv_inst_t *inst)
{
	return (inst ? inst : default_drv_inst);
}

static int
kdbg_drv_open(struct inode *inode, struct file *file)
{
	try_module_get(THIS_MODULE);
	file->private_data = drv_inst_alloc(file);
	if (!file->private_data) {
		module_put(THIS_MODULE);
		return (-ENOMEM);
	}
	return (0);
}

static int
kdbg_drv_release(struct inode *inode, struct file *file)
{
	drv_inst_free(file->private_data);
	file->private_data = NULL;
	module_put(THIS_MODULE);
	return (0);
}

static ssize_t
kdbg_drv_read(struct file *file, char __user *buf, size_t len, loff_t *offset)
{
	drv_inst_t *inst = file->private_data;
	if (*offset) {
		if (*offset == 1)
			inst = default_drv_inst ?: inst;
		else
			kdbg_log("Read by offset=%lld, len=%ld", *offset, len);
	}

	int rc = kdbg_read_screen(file->private_data, buf, len);
	if (rc < 0)
		kdbg_log("Failed to copy to user-buf, "
		    "buf=0x%llx, len=%ld, rc=%d", (uint64_t)buf, len, rc);

	return (rc);
}

static ssize_t
kdbg_drv_write(struct file *file,
    const char __user *buf, size_t len, loff_t *offset)
{
	drv_inst_t *inst = file->private_data;
	if (!ksym_imported()) {
		kdbg_print(inst, "There are some symbols not imported.\n");
		kdbg_print(inst, "Run command 'scripts/ksym.py %s.ko' "
		    "and try again\n", KDBG_DEVNAME);
		return (-EPERM);
	}

	int rc = drv_inst_set_usrcmd(file->private_data, buf, len);
	if (!rc)
		rc = kdbg_main(file->private_data);
	return (rc ? rc : len);
}

static loff_t
kdbg_drv_seek(struct file *file, loff_t off, int whence)
{
	loff_t new_off;

	switch (whence) {
	case SEEK_SET:
		new_off = off;
		/* kdbg_log("Seek off: SET %lld", off); */
		break;

	case SEEK_CUR:
		new_off = file->f_pos + off;
		/* kdbg_log("Seek off: CUR %lld + %lld => %lld",
		    file->f_pos, off, new_off); */
		break;

	case SEEK_END:
		new_off = (loff_t)((uint64_t)-1 >> 1) + off;
		kdbg_log("Seek off: END %lld + %lld => %lld",
		    (loff_t)((uint64_t)-1 >> 1), off, new_off);
		break;

	default:
		return (-EINVAL);
	}

	if (new_off < 0)
		return (-EINVAL);

	file->f_pos = new_off;
	return (new_off);
}

static int kdbg_drv_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	add_uevent_var(env, "DEVMODE=%#o", 0666);
	return (0);
}

static drv_ctrl_t drv_ctrl = {
	.fops = {
		.owner		= THIS_MODULE,
		.open		= kdbg_drv_open,
		.release	= kdbg_drv_release,
		.read		= kdbg_drv_read,
		.write		= kdbg_drv_write,
		.llseek		= kdbg_drv_seek
	},
	.uevent = kdbg_drv_uevent
};

static int
drv_ctrl_init(void)
{
	drv_ctrl_t *ctrl = &drv_ctrl;
	if (alloc_chrdev_region(&ctrl->dev, 0, 1, KDBG_DEVNAME) < 0) {
		kdbg_log("alloc major number error");
		return (-1);
	}
	ctrl->state |= STATE_ALLOC_CDEV;

	cdev_init(&ctrl->cdev, &ctrl->fops);
	ctrl->cdev.owner = THIS_MODULE;
	ctrl->cdev.ops = &ctrl->fops;

	if (cdev_add(&ctrl->cdev, ctrl->dev, 1) < 0) {
		kdbg_log("add dev error");
		return (-1);
	}
	ctrl->state |= STATE_ADD_CDEV;

	ctrl->dev_class = class_create(THIS_MODULE, KDBG_DEVNAME);
	if (IS_ERR(ctrl->dev_class)) {
		kdbg_log("create the struct class error");
		return (-1);
	}
	ctrl->state |= STATE_CREATE_CLASS;

	if (ctrl->uevent)
		ctrl->dev_class->dev_uevent = ctrl->uevent;

	ctrl->device = device_create(ctrl->dev_class,
	    NULL, ctrl->dev, NULL, KDBG_DEVNAME);
	if (IS_ERR(ctrl->device)) {
		kdbg_log("create device error");
		return (-1);
	}
	ctrl->state |= STATE_CREATE_DEV;

	return (0);
}

#define drv_ctrl_cleanup(ctrl, flag, do_cleanup)			\
	do {								\
		if ((ctrl)->state & flag) {				\
			(ctrl)->state &= ~(flag);			\
			do_cleanup					\
		}							\
	} while (0)

static void
drv_ctrl_fini(void)
{
	drv_ctrl_t *ctrl = &drv_ctrl;

	drv_ctrl_cleanup(ctrl, STATE_CREATE_DEV, {
		device_destroy(ctrl->dev_class, ctrl->dev);
	});

	drv_ctrl_cleanup(ctrl, STATE_CREATE_CLASS, {
		class_destroy(ctrl->dev_class);
	});

	drv_ctrl_cleanup(ctrl, STATE_ADD_CDEV, {
		cdev_del(&ctrl->cdev);
	});

	drv_ctrl_cleanup(ctrl, STATE_ALLOC_CDEV, {
		unregister_chrdev_region(ctrl->dev, 1);
	});
}

static int __init
kdbg_drv_init(void)
{
	if (drv_ctrl_init()) {
		drv_ctrl_fini();
		return (-1);
	}

	default_drv_inst = drv_inst_alloc(NULL);
	kdbg_log("kdbg(name:%s) init", KDBG_DEVNAME);

	kdbg_trace_init();
	kdbg_cmd_table_init();

	return (0);
}

static void kdbg_rele_all_modules(void);

static void __exit
kdbg_drv_exit(void)
{
	kdbg_rele_all_modules();
	drv_ctrl_fini();
	drv_inst_free(default_drv_inst);
	kdbg_log("kdbg(name:%s) exit", KDBG_DEVNAME);
}

#ifndef offsetof
#define offsetof(s,m) ((size_t)&((s*)0)->m)
#endif

typedef struct mod_ref {
	struct mod_ref *	next;
	uint32_t		ref;
	char			name[1];
} mod_ref_t;

typedef struct mod_mgr {
	struct mutex		mtx;
	mod_ref_t *		head;
	uint32_t		local_ref;
} mod_mgr_t;

static mod_ref_t *
get_mod_ref(mod_mgr_t *mgr, const char *mod_name, mod_ref_t ***where, int alloc)
{
	mod_ref_t **iter;
	for (iter = &mgr->head; *iter; iter = &(*iter)->next)
		if (!strcmp((*iter)->name, mod_name))
			return (*(*where = iter));

	if (!alloc) {
		*where = NULL;
		return (NULL);
	}

	int namelen = strlen(mod_name) + 1;
	*iter = kmalloc(offsetof(mod_ref_t,name[namelen]), GFP_KERNEL);
	if (*iter) {
		mod_ref_t *modref = *iter;
		modref->next = NULL;
		modref->ref = 0;
		memcpy(modref->name, mod_name, namelen);
		*where = iter;
	} else {
		kdbg_log("Failed to alloc mod_ref for module(%s)", mod_name);
		*where = NULL;
	}

	return (*iter);
}

static void
put_mod_ref(mod_ref_t **where)
{
	if (!where || (*where)->ref > 0)
		return;
	else {
		mod_ref_t *modref = *where;
		*where = NULL;
		kfree(modref);
	}
}

static int
hold_module(mod_mgr_t *mgr, const char *mod_name)
{
	if (mod_name == KDBG_THIS_MODULE) {
		int ok = !!try_module_get(THIS_MODULE);
		mgr->local_ref += ok;
		return (ok);
	}

	if (!strcmp(mod_name, KDBG_DEVNAME)) {
		kdbg_log("Error: *** getting local module(%s) is forbidden.",
		    mod_name);
		return (0);
	}

	mod_ref_t **where;
	if (!get_mod_ref(mgr, mod_name, &where, 1))
		return (0);

	struct module *mod = find_module(mod_name);
	if (!mod) {
		put_mod_ref(where);
		kdbg_log("Error: *** Failed to find module(%s)\n", mod_name);
		return (0);
	}

	mod_ref_t *modref = *where;
	int ok = !!try_module_get(mod);
	modref->ref += ok;
	put_mod_ref(where);
	return (ok);
}

static void
rele_module(mod_mgr_t *mgr, const char *mod_name)
{
	if (mod_name == KDBG_THIS_MODULE) {
		if (mgr->local_ref > 0) {
			module_put(THIS_MODULE);
			mgr->local_ref--;
		}
		return;
	}

	mod_ref_t **where;
	if (get_mod_ref(mgr, mod_name, &where, 0)) {
		mod_ref_t *modref = *where;
		if (modref->ref > 0) {
			struct module *mod = find_module(mod_name);
			if (mod)
				module_put(mod);
			modref->ref--;
		}
		put_mod_ref(where);
	}
}

static mod_mgr_t mod_mgr = {
	.mtx = __MUTEX_INITIALIZER(mod_mgr.mtx)
};

int
kdbg_hold_module(const char *mod_name)
{
	mod_mgr_t *mgr = &mod_mgr;

	mutex_lock(&mgr->mtx);
	int ok = hold_module(mgr, mod_name);
	mutex_unlock(&mgr->mtx);

	return (ok);
}

void
kdbg_rele_module(const char *mod_name)
{
	mod_mgr_t *mgr = &mod_mgr;

	mutex_lock(&mgr->mtx);
	rele_module(mgr, mod_name);
	mutex_unlock(&mgr->mtx);
}

const char *
kdbg_local_module_name(void)
{
	return (KDBG_DEVNAME);
}

static void
kdbg_rele_all_modules(void)
{
	mod_mgr_t *mgr = &mod_mgr;
	struct module *mod;
	mod_ref_t *modref;

	/* Note: not release references of THIS_MODULE because
	 * kdbg_drv_exit() would not be called if THIS_MODULE is in use
	 * and this function is called in kdbg_drv_exit()
	 */
	while (!!(modref = mgr->head)) {
		mgr->head = modref->next;
		mod = find_module(modref->name);
		if (mod) {
			while (modref->ref > 0) {
				module_put(mod);
				modref->ref--;
			}
		}
		kfree(modref);
	}
}

module_init(kdbg_drv_init);
module_exit(kdbg_drv_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zhengyp");
MODULE_DESCRIPTION("kernel debugger");
MODULE_VERSION("1.0");
