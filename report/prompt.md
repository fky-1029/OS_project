# AI提示词

## 功能模块一

**负责人：** 2413406-黄婉文

````markdown

[PROMPT]
**任务**：修改 kern/trap/trap.c、kern/syscall/syscall.c，实现用户态系统调用功能模块。需要实现或补全以下函数：

void exception_handler(struct trapframe *tf);
void syscall(void);
static int sys_exit(uint64_t arg[]);
static int sys_fork(uint64_t arg[]);
static int sys_wait(uint64_t arg[]);
static int sys_exec(uint64_t arg[]);
static int sys_yield(uint64_t arg[]);
static int sys_kill(uint64_t arg[]);
static int sys_getpid(uint64_t arg[]);
static int sys_putc(uint64_t arg[]);
static int sys_pgdir(uint64_t arg[]);

该模块负责把用户态通过 ecall 发起的请求接入内核，再把内核处理结果写回用户态寄存器。分工如下：

- exception_handler 识别 CAUSE_USER_ECALL 用户态系统调用异常，转交 syscall，并把 epc 前移，避免返回后重复执行同一条 ecall。
- syscall 从当前进程 trapframe 取出系统调用号和参数，按统一编号分发到各 sys_* 函数，并把返回值写回用户可见返回寄存器。
- 各 sys_* 函数把统一编号的系统调用请求转换成具体内核语义，调用 do_exit、do_fork、do_wait、do_execve、do_yield、do_kill 等进程管理层入口。
- RESTORE_ALL 宏负责异常返回时恢复完整上下文，并根据 sstatus 中保存的返回特权级信息正确执行 sret，使用户态与内核态异常返回都能走同一条恢复路径。

完成后，用户程序应能通过用户库封装发起系统调用，内核能正确接收、分发、处理并返回；ecall 进入内核后不会因 epc 未前移而反复触发；返回用户态前寄存器、sepc、sstatus 均被正确恢复。

**操作要求**：必须在项目中修改实际文件，而不是仅展示代码。保留现有代码，只在需要实现或补全的位置修改；不要删除既有逻辑，不要引入与当前实验无关的功能。需要完整实现函数主体、必要的静态辅助函数、系统调用分发表、模块内部静态变量等。

**输出要求**：直接进行文件操作。使用下面 [RELY]、[GUARANTEE]、[SPECIFICATION] 中的信息；以工程真实代码为准，不要凭记忆臆造宏名、结构体字段、系统调用号或函数签名。实现应能通过编译，并满足本实验对用户态系统调用进入、分发、处理、返回的行为要求。

[RELY]
关键宏与常量：CAUSE_USER_ECALL；SYS_exit、SYS_fork、SYS_wait、SYS_exec、SYS_yield、SYS_kill、SYS_getpid、SYS_putc、SYS_pgdir；E_INVAL、E_NO_MEM、E_UNIMP 等错误码；PTE_U、PTE_R/W/X/V 等页表权限位；SSTATUS_SPP、SSTATUS_SPIE、SSTATUS_SIE。以上以工程 error.h、unistd.h、trap.h、riscv.h 为准。

关键数据结构：struct trapframe，包含通用寄存器（如 gpr.a0~a5、gpr.sp、gpr.ra 等）以及 status、sepc、badaddr、cause 等 CSR 子集，字段名以工程为准；struct pushregs；struct proc_struct，包含 tf、mm、pid、state 等字段；struct mm_struct；全局变量 current。

可调用已有函数/符号：do_exit(int error_code)；do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf)；do_wait(int pid, int *code_store)；do_execve(const char *name, size_t len, unsigned char *binary, size_t size)；do_yield(void)；do_kill(int pid)；user_mem_check(struct mm_struct *mm, uintptr_t addr, size_t len, bool write)；cputchar(int c)；print_trapframe(struct trapframe *tf)；print_pgdir(void) 或工程中页表打印接口；current、current->tf、current->mm、current->pid。

[GUARANTEE]
必须实现或修改的接口：

void exception_handler(struct trapframe *tf);
void syscall(void);
static int sys_exit(uint64_t arg[]);
static int sys_fork(uint64_t arg[]);
static int sys_wait(uint64_t arg[]);
static int sys_exec(uint64_t arg[]);
static int sys_yield(uint64_t arg[]);
static int sys_kill(uint64_t arg[]);
static int sys_getpid(uint64_t arg[]);
static int sys_putc(uint64_t arg[]);
static int sys_pgdir(uint64_t arg[]);

可根据需要添加：
- 系统调用分发表或分发辅助函数，建议设为 static。
- 用户参数检查辅助函数，建议设为 static。
- 模块内部使用的宏定义。
- 模块内部静态变量。

**模块对外接口要求**：
- 用户态需要看到语义清晰的库函数接口，如 exit、fork、wait、waitpid、exec、yield、kill、getpid、cprintf 以及页表查看相关接口。
- 内核态需要提供统一系统调用入口 exception_handler 和 syscall，并保证每个系统调用返回时把结果正确放入用户可见返回寄存器。
- 对外表现上，用户程序应像在普通操作系统中一样使用系统调用，而不需要关心异常入口、寄存器传参和 trap 返回细节。

[SPECIFICATION]

exception_handler
- 入参 tf 有效，current 有效。
- 若异常为 CAUSE_USER_ECALL，调用 syscall()，并将 tf 中 epc 前移，通常加 4。
- 其他异常按工程原有逻辑处理，不得误把内核异常当作用户系统调用。
- 不得在此函数中直接实现具体系统调用语义。

syscall
- 由 exception_handler 在用户态 ecall 分支调用。
- 从 current->tf 取系统调用号和参数，按编号分发到对应 sys_* 函数。
- 将返回值写回用户可见返回寄存器，通常为 a0。
- 非法或未实现编号返回明确错误码，不得越界访问分发表。

sys_exit
- 调用 do_exit((int)arg[0])。
- 不得直接修改进程状态或释放资源。

sys_fork
- 以工程约定的 clone_flags 和 current->tf 调用 do_fork。
- 返回 do_fork 的返回值；失败返回负错误码。
- 不得自行复制页表或进程结构。

sys_wait
- arg[0] 为 pid，arg[1] 为用户退出码存放地址。
- 若 arg[1] 非 0，先做 user_mem_check 确认可写。
- 调用 do_wait((int)arg[0], (int *)arg[1])。
- 用户地址非法时返回 -E_INVAL。

sys_exec
- 参数顺序以工程和用户库封装为准，通常包括程序名、名字长度、二进制地址、二进制大小。
- 对程序名、二进制等用户指针做 user_mem_check 读权限检查。
- 调用 do_execve(name, len, binary, size)。
- 用户指针非法时返回 -E_INVAL；不得在此函数中直接解析 ELF 或替换页表。

sys_yield
- 调用 do_yield() 并返回结果。
- 不得直接修改 need_resched 或调用调度器内部函数。

sys_kill
- 调用 do_kill((int)arg[0]) 并返回结果。
- 不得直接修改目标进程状态位。

sys_getpid
- 返回 current->pid。
- 按工程约定处理内核线程 PID 语义。

sys_putc
- 调用 cputchar((int)arg[0])，返回 0 或工程约定值。
- 不得直接操作串口或 SBI 细节。

sys_pgdir
- 调用工程中的页表打印接口，如 print_pgdir()，返回 0 或工程约定值。
- 优先复用工程已有接口，不重复实现页表遍历打印。

**注意事项**：
- 系统调用编号和参数位置必须与用户态封装一致。通常 a0 保存系统调用号，a1 到 a5 保存参数，返回值写回 a0；具体以 libs/unistd.h 和 user/libs/syscall.c 为准。
- exception_handler 识别到 CAUSE_USER_ECALL 后必须把 epc 前移，通常加 4，否则返回用户态后会重复执行同一条 ecall。
- syscall 分发表必须边界正确，未实现或非法编号不能被当作有效请求处理；分发前要保证 current 和 current->tf 有效。
- 涉及用户指针的参数（如 exec 的程序名/二进制、wait 的退出码存放地址等）不能直接信任，必须结合 user_mem_check 和当前进程地址空间做合法性判断。
- 会改变进程状态、进程树关系或页表内容的处理，都要和当前进程的 trapframe、mm_struct、proc_struct 保持同步，避免返回值已写回但进程上下文未切换好。

````

## 功能模块二


## 功能模块三


## 功能模块四


## challenge 1