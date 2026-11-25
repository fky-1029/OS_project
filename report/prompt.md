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

**负责人：** 2413406-黄婉文

### prompt 1：COW机制实现

````markdown

[PROMPT]
**任务**：在已实现 fork 全量复制（do_fork → copy_mm → dup_mmap → copy_range）的基础上，实现 Copy on Write（COW）机制。需要修改或补充以下位置：

- kern/mm/mmu.h：新增软件 COW 标志 PTE_COW，并确认 PTE 权限掩码。
- kern/mm/pmm.c：修改 copy_range，增加 COW 共享分支与只读共享分支。
- kern/mm/vmm.c：修改 dup_mmap，使其在 COW fork 时让 copy_range 走共享路径；补充 do_pgfault，处理 COW 写缺页。
- kern/mm/vmm.h：增加 do_pgfault 的声明。
- kern/process/proc.c：修改 copy_mm，普通用户进程 fork 时启用 COW；内核线程或 CLONE_VM 路径保持原逻辑。
- kern/trap/trap.c：exception_handler 识别 CAUSE_STORE_PAGE_FAULT，并转交 do_pgfault。

COW 的核心语义是：fork 时可写用户页不复制，父子共享同一物理页，双方 PTE 清写位并设 PTE_COW，物理页引用计数加一；任一方写该页触发 store page fault，内核判断为 COW 页后，ref > 1 则分配新页复制内容并替换写方映射，ref == 1 则直接恢复写权限；只读 vma 直接共享只读页，不设 COW。fork 修改父 PTE 后必须刷新父进程 TLB；写 fault 替换或恢复 PTE 后必须刷新当前进程 TLB；所有 PTE 修改与引用计数调整必须在关中断或持锁的临界区内完成。

**操作要求**：必须在项目中修改实际文件，而不是仅展示代码。保留现有代码，只在需要实现或补全的位置修改；不要删除既有逻辑，不要引入与当前实验无关的功能。需要完整实现 COW 分支、必要的静态辅助函数和缺页处理逻辑。修改 copy_range 时必须与工程现有的 page_insert 引用计数语义保持一致，明确“谁负责 ref 加一、谁负责 ref 减一”，不得出现重复加一或漏减。

**输出要求**：直接进行文件操作。使用下面 [RELY]、[GUARANTEE]、[SPECIFICATION] 中的信息；以工程真实代码为准，不要凭记忆臆造宏名、结构体字段或函数签名。实现应能通过编译，并满足本实验对 COW fork、写时复制、只读共享和引用计数的行为要求。

[RELY]
// 关键宏与常量
PTE_V、PTE_R、PTE_W、PTE_X、PTE_U
PTE_COW（新增软件位，建议 0x100，即 RSW bit 8）
PGSIZE
VM_WRITE
CAUSE_STORE_PAGE_FAULT
E_NO_MEM、E_FAULT、E_INVAL

以上以工程 libs/riscv.h、kern/mm/mmu.h、kern/mm/vmm.h、kern/trap/trap.h、libs/error.h 为准。

// 关键数据结构
struct Page：包含 ref、flags、property、page_link
struct mm_struct：包含 mmap_list、mmap_cache、pgdir、map_count、mm_count、mm_lock
struct vma_struct：包含 vm_start、vm_end、vm_flags
struct trapframe：包含 gpr、status、epc、tval、cause
pde_t、pte_t

// 可调用的已有函数/符号
页表与页面：
get_pte、page_insert、pte2page、page2kva、page2pa、alloc_page、free_page、
page_ref_inc、page_ref_dec、tlb_invalidate、memcpy、flush_tlb

地址空间：
mm_create、setup_pgdir、put_pgdir、dup_mmap、copy_range、
find_vma、lock_mm、unlock_mm、exit_mmap、mm_destroy

进程与异常：
do_fork、copy_thread、do_exit、current、current->mm、current->tf、current->pid、
local_intr_save、local_intr_restore

// 引用计数职责
page_insert 负责：
- 新页 ref 加一；
- 替换 PTE；
- 旧页与当前 PTE 不同且旧页存在时，旧页 ref 减一。

copy_range 子进程映射时只调用 page_insert，不再手动 page_ref_inc。
do_pgfault 慢路径也只调用 page_insert，不再额外调用 page_remove_pte。

// 地址空间约定
mm->pgdir 是 pde_t *，供 get_pte、page_insert、copy_range、do_pgfault 使用。
proc->pgdir 是 uintptr_t 物理地址，供 proc_run 写 satp 使用。
不得把 current->pgdir 传给 page_insert 或 copy_range。

[GUARANTEE]
必须实现或修改的接口：

```c
// kern/mm/mmu.h
#define PTE_COW 0x100
// kern/mm/pmm.c 
int copy_range(pde_t *to, pde_t *from,uintptr_t start, uintptr_t end, bool share);
// kern/mm/vmm.c 
int dup_mmap(struct mm_struct *to, struct mm_struct *from);
int do_pgfault(struct mm_struct *mm, uint32_t cause, uintptr_t addr);
// kern/mm/vmm.h 
int do_pgfault(struct mm_struct *mm, uint32_t cause, uintptr_t addr);
// kern/process/proc.c 
static int copy_mm(uint32_t clone_flags, struct proc_struct *proc);
// kern/trap/trap.c 
void exception_handler(struct trapframe *tf);
```

你可以根据需要添加：

- COW 相关静态辅助函数，建议设为 static。
- 模块内部使用的宏定义。
- 模块内部静态变量。

**模块对外接口要求**：
- fork 语义不变：父进程返回子进程 PID，子进程从 0 返回，父子地址空间逻辑独立，写互不可见。
- 只读页在 fork 后可共享，不产生多余复制。
- 写 COW 页时能正确触发 page fault 并完成按需复制，后续写入不再触发 fault。
- 引用计数正确：每增加一个映射 ref 加一，每删除一个映射 ref 减一，共享页在最后一位持有者释放时才真正释放。

[SPECIFICATION]

1. 普通 fork 后，父子进程写同一全局变量互不影响。
2. 子进程写 COW 页时触发一次 store page fault，复制后再次写入不再触发 fault。
3. 父进程写 COW 页时同样触发一次 fault，子进程数据不受影响。
4. 只读代码页 fork 后共享只读，不设置 PTE_COW；写代码页按非法访问处理。
5. 多个子进程共享同一页时，ref 正确增加；子进程退出后 ref 正确减少。
6. 只剩最后一个 COW 映射时，写操作不分配新页，直接恢复可写。
7. 所有进程退出后，COW 页和普通页都能正确释放，nr_free_pages 恢复。
8. forktest、forktree、exit 等现有用户程序仍能通过。

````

### prompt 2：COW 测试

````markdown
[PROMPT]
**任务**：为 COW 机制生成测试接口和用例代码，覆盖[SPECIFICATION]中的场景，并在代码中生成相应测试接口。

**操作要求**：必须在项目中新增或修改实际文件，而不是仅展示代码。用户态测试程序用 cprintf 输出测试名和 PASS/FAIL，任一测试失败时通过 exit 返回非 0；内核态自检函数风格参照 default_check / buddy_check，失败时 assert 终止，成功时输出 cow_check() succeeded!。如果提示词中有内容与代码框架冲突，以现有代码为准。

**输出要求**：直接进行文件操作。使用下面 [RELY]、[GUARANTEE]、[SPECIFICATION] 中的信息；以工程真实代码为准，不要凭记忆臆造函数名或结构体字段。测试代码必须只通过公开接口访问 COW 行为，不得绕过 COW 逻辑直接读写页表或篡改引用计数。

[RELY]
关键宏与常量：PGSIZE；VM_WRITE；CAUSE_STORE_PAGEFAULT；SYS_exit、SYS_fork、SYS_wait、SYS_getpid、SYS_yield；PROC_RUNNABLE、PROC_ZOMBIE；CLONE_VM、CLONE_FS、CLONE_FILES 等 clone 标志（以工程定义为准）。以上以工程 riscv.h、vmm.h、unistd.h、proc.h 为准。

关键数据结构：struct Page（含 ref、flags）；struct mm_struct；struct vma_struct；struct proc_struct（含 mm、pgdir、pid、parent）；struct trapframe。

可调用函数/符号：用户态可用 fork、exit、wait、getpid、cprintf、memset、memcpy；内核态可用 nr_free_pages()、alloc_page()、free_page()、copy_mm()、dup_mmap()、copy_range()、do_fork()、do_exit()、do_wait()、page_ref_inc()、page_ref_dec()、get_pte()、pte2page()、tlb_invalidate()、current、current->mm、current->pgdir、cprintf、assert。

[GUARANTEE]
必须实现或修改的接口：

```c
// user/cow_test.c（新增）
int main(void);
static void cow_test_**(void);    // 用户态测试程序

// kern/process/proc.c
// 改为 kernel_execve 启动 __user_cow_test.out，或在 user_main 中直接调用内联测试函数
static int user_main(void *arg);

// cow_check 入口与子测试
void cow_check(void);      // COW 自检入口
static void cow_check_**(void);    // 内核态自检函数
```

可根据需要添加：
- 测试用 static 辅助函数和宏。
- 测试用静态变量。
- 用户态测试程序的私有全局变量和数组。

**模块对外接口要求**：
- 用户态测试程序应输出每条测试的名称和 PASS/FAIL，全部通过时 exit(0)，任一失败时 exit(非 0)。
- 内核态 cow_check() 应输出 cow_check() succeeded!，任一断言失败时通过 assert 终止并打印失败位置。
- 现有测试程序在 COW 启用后应继续正确运行。

[SPECIFICATION]

- 父子双向写隔离：fork 后子进程写共享变量父进程应看到旧值，父进程写子进程也应看到旧值。
- 首次写触发缺页：COW 页第一次写应触发一次页错误，第二次写同一页不再触发。
- 只读页共享：fork 后读代码段或只读数据不复制，写只读页按非法访问处理。
- 最后一个 COW 映射：子进程先退出、父进程再写时，应直接恢复可写且不分配新页。
- 多共享者：多个子进程共享同一页时，一个子进程写不应影响其他子进程和父进程。
- 嵌套 fork：在已经是 COW 的页上再次 fork，引用计数和共享关系应保持正确。
- 多页与边界：数据段、BSS、栈、页内偏移、跨页写都应整页复制且内容正确。
- 生命周期：反复 fork、写、退出后空闲页数量应恢复，不能泄漏或重复释放。
- 失败路径：COW 分配新页失败或 fork 中途失败时，进程应安全退出且旧页不被破坏。
- 分支路径：CLONE_VM 线程共享写应直接可见，内核线程 fork 不应进入 COW。
- TLB 正确性：父进程 fork 后立即写共享页必须触发缺页，否则说明父 TLB 未刷新。
- 回归测试：exit、forktest、forktree、spin 等现有程序应继续通过。

**注意事项**：
- 用户态测试只能通过 fork、exit、wait、写共享变量、cprintf 等公开接口观察行为，不得直接访问页表、物理页号或引用计数。
- 页错误次数、复制次数、引用计数、TLB 是否刷新、失败路径、CLONE_VM 分支这些底层机制无法在用户态可靠观测，应放在内核态自检中完成。
- 用户态测试程序需通过 Makefile 编译并由 kernel 以嵌入二进制方式加载；若工程中新增用户程序的方式与现有 user/ 目录一致，照搬现有 Makefile 规则即可。

````
