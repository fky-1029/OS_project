# 操作系统实验报告

## 实验基本信息

| 项目 | 内容 |
|------|------|
| **实验名称** | Lab 5: 用户程序 |
| **小组成员** | 2413406-黄婉文、2413396-范珂祎、2413671-吕旭婧 |
| **完成日期** | YYYY-MM-DD |

### 小组分工

练习如何分工？

| 成员 | 负责的练习/模块 |
|------|----------------|
| 2413406-黄婉文 | 功能模块一，分支任务，challenge |
| 2413396-范珂祎 | [练习或模块名称] |
| 2413671-吕旭婧 | [练习或模块名称] |

实验报告如何分工？

| 成员 | 负责的报告部分 |
|------|----------------|
| 2413406-黄婉文 | 功能模块一，分支任务，challenge，代码迁移 |
| 2413396-范珂祎 | [练习或模块名称] |
| 2413671-吕旭婧 | [练习或模块名称] |

---

## 一、实验目的

<!-- 说明本次实验的主要目标和预期成果 -->

本实验的主要目的是：

1. 了解第一个用户进程创建过程；
2. 了解系统调用框架的实现机制；
3. 了解ucore如何实现系统调用sys_fork/sys_exec/sys_exit/sys_wait来进行进程管理。

---

## 二、实验环境

你们使用的 AI 工具

| 成员 | AI 编程工具 | 底层模型 | 备注 |
|------|------------|---------|------|
| 2413406-黄婉文 | DeepSeek Harness | DeepSeek-Flash | 无 |
| 2413396-范珂祎 | | | |
| 2413671-吕旭婧 | | | |

**说明：**
- **AI 编程工具**：指具体使用的终端工具、编辑器插件、桌面应用或浏览器界面
- **底层模型**：指该工具使用的大语言模型及版本

---

## 三、实验整体逻辑分析

### 2.1 本章节的逻辑主线

本实验需要构造用户进程，让用户程序真正运行在用户态，并在需要操作系统支持时，通过系统调用进入内核获得服务。需要完成的模块包括系统调用入口、首个用户进程的创建与装载、用户进程的复制、以及退出、让出和终止等进程控制功能，从而把“启动内核线程”推进到“运行和管理用户程序”的完整阶段。

整个实验要解决的核心问题是用户程序如何被操作系统加载、启动、运行，并通过系统调用与内核交互，最后再正确返回用户态，主线可以概括为：准备用户程序映像、真正进入用户态、用户态与内核态的切换。

### 2.2 功能的逐步实现

本章的实现顺序由依赖关系决定：先让内核具备运行基础，再打通用户态进入内核的入口，接着让第一个用户程序真正跑起来，然后让用户进程能够被复制，最后让进程能够退出并被回收。

1. 首先完成 Lab2/3/4 代码迁移，并把 RESTORE_ALL 扩展为支持用户态与内核态双向返回的完整版本。Lab5 建立在物理内存管理、页表映射、中断异常处理、内核线程的基础之上；用户程序有独立的用户栈，中断返回时必须根据 sstatus.SPP 判断本次是返回 U 态还是 S 态，并在返回用户态前把内核栈顶写入 sscratch，否则用户态下一次中断异常会把 tf 分配到用户栈上。

2. 接着实现用户态系统调用功能，打通 ecall → exception_handler → syscall → sys_* 的入口链路。用户程序运行在 U 态，所有系统服务请求都必须通过 ecall 进入内核，因此要先让 exception_handler 识别 CAUSE_USER_ECALL，同时 epc 前移，再交给 syscall 按系统调用号分发到对应函数，并把返回值写回 tf->gpr.a0。这是后续所有用户态功能的公共入口。

3. 然后创建并加载第一个用户进程，完成 init_main → user_main → kernel_execve → do_execve → load_icode → sret 的启动链。系统调用入口准备好后，init_main 先创建 user_main 内核线程，user_main 通过 KERNEL_EXECVE 宏选择嵌入内核镜像的用户程序；kernel_execve 负责构造返回用户态所需的 trapframe，do_execve 回收旧地址空间，load_icode 解析 ELF、建立新的 VMA 和页表、装入代码段和数据段、创建用户栈，并设置用户入口和用户栈指针。这一步完成后，系统才能第一次真正运行用户程序。

4. 实现用户进程复制功能，完成 do_fork → copy_mm → dup_mmap → copy_range 的地址空间复制链路。do_fork 负责分配 PCB、内核栈、PID，建立进程树关系并唤醒新进程；copy_mm 负责判断是共享父进程地址空间还是复制一份新的地址空间；dup_mmap 按 VMA 区间复制合法的用户虚拟内存区域；copy_range 则按页复制父进程的页面内容，并在子进程页表中建立新的映射，同时正确处理权限位和页引用计数。

5. 最后补全进程退出、等待、让出和终止功能，实现完整的进程生命周期。do_exit 负责释放当前进程的用户地址空间和页表，把进程置为 PROC_ZOMBIE 并记录退出码，同时把子进程挂到 initproc 名下、唤醒等待的父进程，最后调用 schedule；do_wait 负责在父进程中回收僵尸子进程，释放其内核栈和 PCB；do_yield 把当前进程标记为需要重新调度；do_kill 给目标进程设置 PF_EXITING 并在必要时唤醒它。

---


## 四、实验内容与实现

<!-- 
说明：本部分按照 exercises 文档中的练习顺序组织
每个功能模块或者练习中可能包含多项任务，请根据实际情况调整
-->

### 代码迁移

**负责人：** 2413406-黄婉文

本实验在lab2/lab3/lab4的代码基础上完成，需要用此前设计的提示词生成相应实验代码并填到注释的对应位置，确保代码能够正确编译。另外原来的 RESTORE_ALL 宏仅能用于纯内核、无用户态的系统，需要修改扩充为支持用户态/内核态双向异常返回的完整版上下文恢复宏。

**需要实现/修改的函数：**

```c
// kern/mm/default_pmm.c
static void default_init(void);
static void default_init_memmap(struct Page *base, size_t n);
static struct Page *default_alloc_pages(size_t n);
static void default_free_pages(struct Page *base, size_t n);
static size_t default_nr_free_pages(void);

// kern/mm/pmm.c
static void page_init(void);
static void page_mapping_invalidate(pde_t *pgdir, uintptr_t la, bool table_entries_may_have_changed);
static void page_mapping_put(struct Page *page);
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm);
void page_remove(pde_t *pgdir, uintptr_t la);

// kern/trap/trap.c
void interrupt_handler(struct trapframe *tf);
void exception_handler(struct trapframe *tf);

// kern/trap/trapentry.S
RESTORE_ALL：支持用户态/内核态双向异常返回
void __alltraps(void);

// kern/process/proc.c 
static int init_main(void *arg);
static struct proc_struct *alloc_proc(void);
static void copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf);
static int copy_mm(uint32_t clone_flags, struct proc_struct *proc);
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf);
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags);
void proc_init(void);
int do_exit(int error_code);
void cpu_idle(void);
void proc_run(struct proc_struct *proc);

// kern/schedule/sched.c
void schedule(void);

// kern/process/switch.S
void switch_to(struct context *from, struct context *to);
```

其中，RESTORE_ALL 会先根据 tf->status 中的 SSTATUS_SPP 判断本次返回的目标特权级：若返回内核态，则保持 SAVE_ALL 设置的 sscratch = 0；若返回用户态，则把当前内核栈顶 sp + 36*REGBYTES 写回 sscratch，以便用户态下一次 trap 时 SAVE_ALL 能通过 csrrw sp, sscratch, sp 从用户栈切换到内核栈，最后再从 trapframe 恢复用户 sp；同时，恢复 sstatus、sepc 的临时寄存器由 t0 改为不会被提前覆盖的 s1 和 s2，而 x2、sp 仍在最后恢复。


### 功能模块一：用户态系统调用功能

**负责人：** 2413406-黄婉文

#### 模块功能描述

**需要实现/修改的函数：**

```c
// kern/trap/trap.c
void exception_handler(struct trapframe *tf);

// kern/syscall/syscall.c
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
```

**功能说明：**

本模块是用户态程序进入内核的公共入口，用户库按照约定把系统调用号放入 a0、参数放入 a1~a5，然后执行 ecall；trapentry.S 负责保存现场并切换到内核栈，trap.c 识别 CAUSE_USER_ECALL，先将 epc 前移 4 字节，再调用 syscall；syscall.c 从 current->tf 中取出调用号和参数，按编号查表分发到 sys_exit、sys_fork、sys_wait、sys_exec、sys_yield、sys_kill、sys_getpid、sys_putc、sys_pgdir；各 sys_* 调用 proc.c 中的 do_exit、do_fork、do_wait、do_execve、do_yield、do_kill 等下层入口，以及 cputchar、页表查看等已有接口。处理完成后，返回值写回 tf->gpr.a0，异常返回路径恢复 sstatus、sepc 和通用寄存器，最后由 sret 回到用户态继续执行。

需要处理的主要场景包括：

- 普通服务调用：getpid 获取当前进程号，putc 向控制台输出字符，pgdir 查看页表信息；
- 进程控制调用：exit 退出当前进程，fork 复制子进程，wait 等待并回收子进程，exec 替换当前进程映像，yield 主动让出 CPU，kill 终止指定进程；
- 异常与边界处理：非法或未实现的系统调用号返回 -E_INVAL，不越界访问分发表；wait 的退出码地址、exec 的程序名和二进制地址等用户指针必须经过 user_mem_check 校验，非法地址返回 -E_INVAL；
- 退出返回：fork 在父进程中返回子进程 PID，在子进程中返回 0；exit 成功进入退出流程后不再返回；exec 成功后当前进程开始执行新程序；其他调用把结果写回 a0 后继续执行。


#### 最终提示词

以下是经过迭代优化后，最终成功实现该功能的提示词：

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

#### 实现迭代过程

本模块的实现未经历迭代，初版提示词即实现模块所有相关函数，在所有模块完成后进行编译测试。

---


### 功能模块二：创建加载第一个用户进程功能

**负责人：** [学号-姓名]

#### 模块功能描述

**需要实现/修改的函数：**

```c
// 列出本模块涉及的所有函数签名
void example_function(void);
int another_function(int arg);
```

**功能说明：**

[简要描述该模块的功能，包括：
- 该模块在整个系统中的作用
- 需要处理的主要场景
- 与其他模块的交互关系]

#### 最终提示词

以下是经过迭代优化后，最终成功实现该功能的提示词：

````markdown

[按照提示词模板，给出具体的提示词]

````

#### 实现迭代过程

本模块的实现经历了 [N] 次迭代，过程如下：

---

##### 第一次迭代

如果设计的prompt生成代码后出现问题：

**遇到的问题：**
- 问题1：问题描述、错误信息和原因分析
- 问题2：问题描述、错误信息和原因分析

**问题解决策略：**
- 针对问题1：[说明如何修改提示词]
- 针对问题2：[说明如何修改提示词]

如果没有问题：

**最终结果：**

经过 [N] 次迭代，该模块最终：
- ✅ 编译通过
- ✅ 所有相关测试通过
- ✅ 功能符合预期

**关键改进点总结（如有）：**
1. [改进点1：例如，在 [RELY] 中补充了完整的系统调用号定义]
2. [改进点2：例如，在 [SPECIFICATION] 中明确了异常处理的边界条件]
3. [改进点3：例如，增加了对用户指针合法性检查的要求]

---

##### 第二次迭代（如有）

[按照和第一次迭代相同的格式]

---

##### 第三次迭代（如有）

[按照和第一次迭代相同的格式]

---



### 功能模块三：复制用户进程功能

**负责人：** [学号-姓名]

#### 模块功能描述

**需要实现/修改的函数：**

```c
// 列出本模块涉及的所有函数签名
void example_function(void);
int another_function(int arg);
```

**功能说明：**

[简要描述该模块的功能，包括：
- 该模块在整个系统中的作用
- 需要处理的主要场景
- 与其他模块的交互关系]

#### 最终提示词

以下是经过迭代优化后，最终成功实现该功能的提示词：

````markdown

[按照提示词模板，给出具体的提示词]

````

#### 实现迭代过程

本模块的实现经历了 [N] 次迭代，过程如下：

---

##### 第一次迭代

如果设计的prompt生成代码后出现问题：

**遇到的问题：**
- 问题1：问题描述、错误信息和原因分析
- 问题2：问题描述、错误信息和原因分析

**问题解决策略：**
- 针对问题1：[说明如何修改提示词]
- 针对问题2：[说明如何修改提示词]

如果没有问题：

**最终结果：**

经过 [N] 次迭代，该模块最终：
- ✅ 编译通过
- ✅ 所有相关测试通过
- ✅ 功能符合预期

**关键改进点总结（如有）：**
1. [改进点1：例如，在 [RELY] 中补充了完整的系统调用号定义]
2. [改进点2：例如，在 [SPECIFICATION] 中明确了异常处理的边界条件]
3. [改进点3：例如，增加了对用户指针合法性检查的要求]

---

##### 第二次迭代（如有）

[按照和第一次迭代相同的格式]

---

##### 第三次迭代（如有）

[按照和第一次迭代相同的格式]

---



### 功能模块四：其余系统调用功能

**负责人：** [学号-姓名]

#### 模块功能描述

**需要实现/修改的函数：**

```c
// 列出本模块涉及的所有函数签名
void example_function(void);
int another_function(int arg);
```

**功能说明：**

[简要描述该模块的功能，包括：
- 该模块在整个系统中的作用
- 需要处理的主要场景
- 与其他模块的交互关系]

#### 最终提示词

以下是经过迭代优化后，最终成功实现该功能的提示词：

````markdown

[按照提示词模板，给出具体的提示词]

````

#### 实现迭代过程

本模块的实现经历了 [N] 次迭代，过程如下：

---

##### 第一次迭代

如果设计的prompt生成代码后出现问题：

**遇到的问题：**
- 问题1：问题描述、错误信息和原因分析
- 问题2：问题描述、错误信息和原因分析

**问题解决策略：**
- 针对问题1：[说明如何修改提示词]
- 针对问题2：[说明如何修改提示词]

如果没有问题：

**最终结果：**

经过 [N] 次迭代，该模块最终：
- ✅ 编译通过
- ✅ 所有相关测试通过
- ✅ 功能符合预期

**关键改进点总结（如有）：**
1. [改进点1：例如，在 [RELY] 中补充了完整的系统调用号定义]
2. [改进点2：例如，在 [SPECIFICATION] 中明确了异常处理的边界条件]
3. [改进点3：例如，增加了对用户指针合法性检查的要求]

---

##### 第二次迭代（如有）

[按照和第一次迭代相同的格式]

---

##### 第三次迭代（如有）

[按照和第一次迭代相同的格式]

---



### Challenge 1：实现 Copy on Write （COW）机制

**负责人：** [学号-姓名]

[按照Challenge的具体要求进行解答]

---


### Challenge 2

**负责人：** [学号-姓名]

[按照Challenge的具体要求进行解答]

---


### 分支任务：gdb 调试系统调用以及返回

**负责人：** [学号-姓名]

[按照Challenge的具体要求进行解答]

---


## 五、测试与验证

<!-- 根据该实验的具体情况，提供完整的测试运行截图，应包含：
- 编译及运行成功的输出（make qemu），可能有多个
- 测试通过的结果（make grade），只有一个
-->

**测试截图：**

![测试结果截图](./images/test_result.png)

---

## 六、实验总结与收获

### 对操作系统的理解

1. 列出你们认为本实验中重要的知识点，以及与对应的 OS 原理中的知识点，并简要说明二者的含义、关系和差异
2. 列出你们认为 OS 原理中很重要、但在本实验中没有对应上的知识点

### AI 协作开发的经验

[写出你们在与 AI 协作过程中实际收获的经验或者感悟]

---

