#ifndef _SYS_KDBG_H
#define _SYS_KDBG_H

void _kdbg_log(const char *, ...)
    __attribute__((format(printf,1,2)));
#define kdbg_log(fmt,...) _kdbg_log("KDBG: "fmt"\n", ##__VA_ARGS__)

struct drv_inst;
void _kdbg_print(struct drv_inst *, const char *, ...)
    __attribute__((format(printf,2,3)));
#define kdbg_print(inst,fmt,...) _kdbg_print(inst,fmt, ##__VA_ARGS__)

int kdbg_copyto(void *, const void *, unsigned long);
int kdbg_copyfrom(void *, const void *, unsigned long);

#endif // _SYS_KDBG_H
