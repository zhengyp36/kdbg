#ifndef _SYS_KDBG_DEF_CMD_H
#define _SYS_KDBG_DEF_CMD_H

#define KDBG_CMD_SEC(sec) __attribute__((section(sec)))

struct drv_inst;

typedef struct kdbg_cmd_impl {
	const char *name;
	const char *help;
	int (*entry)(const struct kdbg_cmd_impl*,
	    struct drv_inst *, int argc, char *argv[]);
} cmd_impl_t;

#define KDBG_CMD_DEF(_name,_help,_arg0,_arg1,_arg2)			\
static int __kdbg_cmd_entry_##_name(const cmd_impl_t *,			\
    struct drv_inst *, int, char *[]);					\
									\
static cmd_impl_t __kdbg_cmd_ctrl_##_name = {				\
	.name  = #_name,						\
	.help  = _help,							\
	.entry = __kdbg_cmd_entry_##_name				\
};									\
									\
KDBG_CMD_SEC("._kdbg.cmd") cmd_impl_t * __kdbg_cmd_ctrl_ptr_##_name =	\
	&__kdbg_cmd_ctrl_##_name;					\
									\
static int __kdbg_cmd_entry_##_name(					\
    const cmd_impl_t *__kdbg_cmd_impl_local_, _arg0, _arg1, _arg2)

#define kdbg_cmd_usage()						\
	kdbg_print(inst,"Usage: kdbg.sh access %s %s\n",		\
	    argv[0], __kdbg_cmd_impl_local_->help)

#define KDBG_CMD_DEF_START()	\
	KDBG_CMD_SEC("._kdbg.cmd") cmd_impl_t *__kdbg_start_cmd
extern cmd_impl_t *__kdbg_start_cmd;

#define KDBG_CMD_DEF_STOP()	\
	KDBG_CMD_SEC("._kdbg.cmd") cmd_impl_t *__kdbg_stop_cmd
extern cmd_impl_t *__kdbg_stop_cmd;

#endif // _SYS_KDBG_DEF_CMD_H
