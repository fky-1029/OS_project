#include <defs.h>
#include <stdio.h>
#include <string.h>
#include <console.h>
#include <kdebug.h>
#include <dtb.h>
#include <pmm.h>
#include <sbi.h>

void module1_extra_tests(void) __attribute__((weak));
int kern_init(void) __attribute__((noreturn));

int kern_init(void)
{
    extern char edata[], end[];
    memset(edata, 0, end - edata);
    dtb_init();
    cons_init();
    cprintf("(THU.CST) os is loading ...\n\n");
    print_kerninfo();
    pmm_init();

    if (module1_extra_tests != NULL)
    {
        module1_extra_tests();
    }
    sbi_shutdown();
    while (1)
    {
        asm volatile("wfi");
    }
}
