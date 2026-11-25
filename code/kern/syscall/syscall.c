#include <unistd.h>
#include <proc.h>
#include <syscall.h>
#include <trap.h>
#include <stdio.h>
#include <pmm.h>
#include <vmm.h>
#include <console.h>
#include <error.h>
#include <assert.h>

static int sys_exit(uint64_t arg[]);
static int sys_fork(uint64_t arg[]);
static int sys_wait(uint64_t arg[]);
static int sys_exec(uint64_t arg[]);
static int sys_yield(uint64_t arg[]);
static int sys_kill(uint64_t arg[]);
static int sys_getpid(uint64_t arg[]);
static int sys_putc(uint64_t arg[]);
static int sys_pgdir(uint64_t arg[]);

static int (*syscalls[])(uint64_t arg[]) = {
    [SYS_exit]              sys_exit,
    [SYS_fork]              sys_fork,
    [SYS_wait]              sys_wait,
    [SYS_exec]              sys_exec,
    [SYS_yield]             sys_yield,
    [SYS_kill]              sys_kill,
    [SYS_getpid]            sys_getpid,
    [SYS_putc]              sys_putc,
    [SYS_pgdir]             sys_pgdir,
};

#define NUM_SYSCALLS        ((sizeof(syscalls)) / (sizeof(syscalls[0])))

//lab5 function1 2413406
// syscall - 系统调用总入口：从当前 trapframe 取出调用号和参数，分发后把返回值写回 a0
void syscall(void)
{
    struct trapframe *tf = current->tf;
    if (tf == NULL)
    {
        return;
    }

    uint64_t num = tf->gpr.a0;
    uint64_t args[5] = {
        tf->gpr.a1,
        tf->gpr.a2,
        tf->gpr.a3,
        tf->gpr.a4,
        tf->gpr.a5,
    };

    int ret = -E_INVAL;
    if (num < NUM_SYSCALLS && syscalls[num] != NULL)
    {
        ret = syscalls[num](args);
    }
    tf->gpr.a0 = ret;
}

// sys_exit - 退出当前进程，实际资源回收和调度交给 do_exit
static int sys_exit(uint64_t arg[])
{
    return do_exit((int)arg[0]);
}

// sys_fork - 以当前 trapframe 和用户栈指针复制出一个子进程
static int sys_fork(uint64_t arg[])
{
    (void)arg;
    struct trapframe *tf = current->tf;
    return do_fork(0, tf->gpr.sp, tf);
}

// sys_wait - 等待子进程退出；store 非空时先检查用户地址可写
static int sys_wait(uint64_t arg[])
{
    int pid = (int)arg[0];
    int *store = (int *)arg[1];
    if (store != NULL &&
        !user_mem_check(current->mm, (uintptr_t)store, sizeof(int), 1))
    {
        return -E_INVAL;
    }
    return do_wait(pid, store);
}

// sys_exec - 校验用户传入的程序名和二进制后，用 do_execve 替换当前进程映像
static int sys_exec(uint64_t arg[])
{
    const char *name = (const char *)arg[0];
    size_t len = (size_t)arg[1];
    unsigned char *binary = (unsigned char *)arg[2];
    size_t size = (size_t)arg[3];

    if (name == NULL || len == 0 || binary == NULL || size == 0)
    {
        return -E_INVAL;
    }
    if (!user_mem_check(current->mm, (uintptr_t)name, len, 0) ||
        !user_mem_check(current->mm, (uintptr_t)binary, size, 0))
    {
        return -E_INVAL;
    }
    return do_execve(name, len, binary, size);
}

// sys_yield - 主动让出 CPU，由 do_yield 设置重新调度标记
static int sys_yield(uint64_t arg[])
{
    (void)arg;
    return do_yield();
}

// sys_kill - 终止指定 pid 的进程，具体状态修改由 do_kill 完成
static int sys_kill(uint64_t arg[])
{
    return do_kill((int)arg[0]);
}

// sys_getpid - 返回当前进程的 pid
static int sys_getpid(uint64_t arg[])
{
    (void)arg;
    return current->pid;
}

// sys_putc - 向控制台输出一个字符
static int sys_putc(uint64_t arg[])
{
    cputchar((int)arg[0]);
    return 0;
}

// sys_pgdir - 打印当前进程的页表根地址，便于观察地址空间
static int sys_pgdir(uint64_t arg[])
{
    (void)arg;
    cprintf("pgdir: 0x%lx\n", current->pgdir);
    return 0;
}
