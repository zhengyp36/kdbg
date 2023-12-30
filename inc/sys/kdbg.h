#ifndef _SYS_KDBG_H
#define _SYS_KDBG_H

typedef struct drv_inst drv_inst_t;

void _kdbg_log(const char *, ...)
    __attribute__((format(printf,1,2)));
#define kdbg_log(fmt,...) _kdbg_log("KDBG: "fmt"\n", ##__VA_ARGS__)

void _kdbg_print(drv_inst_t *, const char *, ...)
    __attribute__((format(printf,2,3)));
#define kdbg_print(inst,fmt,...) _kdbg_print(inst,fmt, ##__VA_ARGS__)

int kdbg_copyto(void *, const void *, unsigned long);
int kdbg_copyfrom(void *, const void *, unsigned long);

#define KDBG_THIS_MODULE ((char*)(unsigned long)-1)

int kdbg_hold_module(const char *mod_name);
void kdbg_rele_module(const char *mod_name);
const char * kdbg_local_module_name(void);

#endif // _SYS_KDBG_H
