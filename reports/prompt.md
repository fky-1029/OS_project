# AI提示词汇总

## 2413671-吕旭婧 · 功能模块一

以下是本模块的用户提示词原文，保留原消息中的转义字符。

### 1. 只读检查

~~~text
[PROMPT：任务与范围]\
\
我负责 uCore Lab4 模块一：虚拟内存页操作。请先只读检查小组仓库的 lab4 分支，明确实现范围和验证办法。\
\
仓库：https\://github.com/fky-1029/OS_project.git\
指导书：D:\lvxujing\Download\CodexHomework\操作系统\lab4 进程管理.txt\
\
先核对远端实际提交号。讨论窗口看到的版本是 e314eca，请以本轮查询为准。建议一个独立的 Lab4 开发目录，不切换 Lab1–Lab3 现有工作区。本轮不修改源码、报告或配置，不提交、不推送。\
\
[RELY：以实际工程为准]\
\
检查 code/kern/mm/pmm.c、pmm.h 和相关页表定义。重点看 page_insert()、page_remove() 的标记及声明，以及 get_pte()、pte_create()、页引用计数、free_page()、tlb_invalidate() 的现有行为。若工程还有其他 Lab 留下的待补项，单独列出来，不混进模块一。\
\
[GUARANTEE：本轮交付]\
\
说明模块一还缺什么、建议改哪里、哪些依赖已经具备；给出单独验证模块一和全组集成验证的可行路线。区分已核实的事实、需要运行才能确认的结果和待组内决定的问题。\
\
[SPECIFICATION：检查重点]\
\
1\. 逐项说明新建映射、给同一物理页更新权限、替换成另一物理页、删除有效映射及重复删除时，PTE、引用计数和 TLB 应分别是什么状态。\
2\. 查看 get_pte(..., 1) 分配失败时有哪些副作用，区分末级映射与中间页表。如果需要改公共接口才能满足更严格的回滚要求，先报告，不直接扩大修改范围。\
3\. 核对 check_pgdir()、check_boot_pgdir() 的调用位置和实际断言，说明它们覆盖了什么、没有覆盖什么。其他模块没齐时，说明如何在独立副本验证我的代码，不改组员代码或已有断言。\
4\. 检查已有 Ubuntu 工具链和 QEMU 是否可用，核对当前 Makefile 的启动和测试入口。不安装软件，不改环境，不执行带批量清理的 clean 或 grade。
~~~

### 2. 实现与单独编译

~~~text
[PROMPT：任务与范围]\
\
根据上一轮检查，实现 Lab4 模块一的 page_insert() 和 page_remove()。\
\
请从小组仓库的 lab4 分支建立独立开发目录：\
D:\lvxujing\Download\CodexHomework\操作系统\lab4\OS_project-lab4-module1\
\
仓库：https\://github.com/fky-1029/OS_project.git\
上一轮核对的基线是 e314eca。先确认远端是否有新提交；保留组员的改动。如果模块一已经有人实现，先说明现状，不覆盖。不要切换 Lab1–Lab3 的目录。\
\
只在 code/kern/mm/pmm.c 的 LAB4 function1 标记区实现两个接口。保留标记，把本处 YOUR CODE 改成我的学号 2413671。需要共用的内部小函数可以放在同一实现区。不改公共 helper、现有测试、其他模块和报告，不提交或推送。\
\
[RELY：已有接口]\
\
以实际工程的 get_pte()、pte_create()、page2ppn()、pte2page()、引用计数接口、free_page()、tlb_invalidate() 和 flush_tlb() 为准。保留现有接口签名和调用者传入的权限语义，不擅自补权限或改测试条件。\
\
get_pte() 在允许创建时可能分配中间页表。它在第二级分配失败时可能保留第一级已挂接的空表，这是现有框架行为。本轮不修改 get_pte() 或承诺完整回滚中间结构；若发现指导书或新版本明确要求完整回滚，先说明差异。\
\
[GUARANTEE：本轮交付]\
\
给出模块一代码差异、实际编译命令和结果，并解释同页改权、换页、撤销映射和分配失败时，PTE、引用计数和 TLB 分别如何处理。说明是否有未通过或未运行的检查。\
\
[SPECIFICATION：行为与验收]\
\
1\. 新建映射成功后，末级 PTE 指向目标物理页，包含有效位和调用者要求的权限，目标页引用计数加一。\
2\. 已经映射到同一物理页时，更新权限但不改变引用计数，不让该页经过回收。换成另一页时，新增新页引用、移除旧页引用；旧页仍被其他地址引用就保留，归零才回收。\
3\. 找不到末级槽位时返回 -E_NO_MEM，不写入末级映射，不改变目标数据页引用计数。说明在分配中间表失败的情况下，框架可能留下什么。\
4\. 删除时只查找，不创建页表。缺表或 PTE 无效时直接返回。删除有效映射后清除 PTE、减少原页引用，归零才回收；普通删除不回收中间页表页。\
5\. 页表更新后处理 TLB 失效，保证旧映射被回收前不再被本 hart 继续使用。区分只修改末级 PTE 与新增非叶表项；新增非叶表项时使用全地址 fence。说明所选的失效策略，不只满足软件断言。\
6\. 在 Ubuntu 的独立 Lab4 目录沿用已有工具链，按 Makefile 的实际参数单独编译 pmm.c 对应的目标文件。保存实际命令和日志。若遇到框架或其他模块的编译问题，只记录，不修改组员代码来绕过。禁止批量删除，不运行带批量清理的 clean 或 grade。\
\
本轮以代码实现和单独编译为验收。
~~~


## 2413406-黄婉文 · 功能模块二

[PROMPT]
**任务**：修改文件 kern/process/proc.c，实现或修改进程创建及初始化模块，包括以下函数：alloc_proc()、proc_init()、kernel_thread()、do_fork()、copy_thread()、copy_mm()。

该模块负责使系统内核能够创建并登记多个内核线程。具体分工如下：
- alloc_proc 负责得到一个干净的进程控制块；
- proc_init 负责把当前执行现场包装成 idleproc，并创建第一个真正的内核线程 initproc；
- kernel_thread 负责把函数入口和参数整理成线程首次运行所需的临时上下文；
- do_fork 负责把线程信息、内核栈、PID、链表、状态位等资源真正组合成一个可运行实体；
- copy_thread 负责把 trapframe 和 context 安放到新线程自己的内核栈上；
- copy_mm 在当前实验阶段承担地址空间处理接口的占位作用。

这个模块完成后，系统里至少要存在 idleproc 和 initproc 两个关键内核线程，current、proc_list、hash_list、nr_process 等全局结构处于可用状态，并且 initproc 需要能够在后续调度中被真正切换执行。

**操作要求**：你必须在项目中修改实际文件，而不是仅展示代码。保留文件中的现有代码，只在需要实现或补全的位置修改；不要删除既有逻辑，不要引入与当前实验无关的功能。需要完整实现函数主体、必要的静态辅助函数、模块内部静态变量等。

**输出要求**：直接进行文件操作。使用下面 [RELY]、[GUARANTEE]、[SPECIFICATION] 中的信息；以工程真实代码为准，不要凭记忆臆造宏名、结构体字段或函数签名。实现应能通过编译，并满足本实验对内核线程创建与初始化的行为要求。

**注意事项**：
- 必须保证 PCB 状态、内核栈、PID、链表节点、哈希表节点和唤醒顺序一致。
- alloc_proc 必须把 PCB 置于安全空白状态，避免后续代码误用未初始化字段。
- proc_init 中的 idleproc 并不是普通 fork 出来的线程，而是直接复用当前执行现场，所以它的 PID、内核栈、调度标记和 current 位置都带有特殊含义。
- kernel_thread 与 copy_thread 必须配合 forkret、forkrets、__trapret 形成正确的首次运行链路。
- do_fork 必须正确处理失败清理、PID 分配、链表挂接和唤醒顺序，否则会出现资源泄漏、僵尸结构或线程状态错误。
- 当前阶段的 copy_mm 不做真正的用户地址空间复制，但仍需要维持函数形态完整，方便后续实验继续扩展。

**模块对外接口要求**：
- alloc_proc 应返回一个已完成基础字段初始化的 PCB。
- proc_init 执行后应保证进程管理全局结构、idleproc、current 和 initproc 都处于可用状态。
- kernel_thread 应把普通函数包装成可创建线程的上下文，并返回新线程 PID。
- do_fork 应返回新线程 PID 或负错误码。
- copy_thread 与 copy_mm 作为内部辅助接口，分别负责首次运行现场和地址空间处理。

[RELY]
// 关键宏与常量
KSTACKSIZE、PGSIZE、PROC_NAME_LEN
MAX_PID、MAX_PROCESS
SSTATUS_SPP、SSTATUS_SPIE、SSTATUS_SIE
PROC_UNINIT、PROC_RUNNABLE、PROC_SLEEPING、PROC_ZOMBIE

// 关键数据结构
struct proc_struct：包含 state、pid、runs、kstack、need_resched、parent、mm、context、tf、pgdir、flags、name、list_link、hash_link
struct context：保存 ra、sp、s0~s11 等被调用者保存寄存器
struct trapframe：保存通用寄存器以及 sepc、sstatus、scause、stval 等陷阱相关 CSR 子集
list_entry_t、proc_list、hash_list
全局变量：current、idleproc、initproc、nr_process

// 可调用的已有函数/符号
内存与字符串：kmalloc()、kfree()、memset()、memcpy()、alloc_pages()、free_pages()、page2kva()、kva2page()
进程管理：set_proc_name()、find_proc()、get_pid()、wakeup_proc()、hash_list_add()、list_init()、list_add()
地址空间/页表：boot_pgdir_pa、boot_pgdir_va、copy_mm() 相关接口
启动与首次运行链路：bootstack、read_csr(sstatus)、kernel_thread_entry、forkret、forkrets
错误码：-E_NO_MEM、-E_INVAL 等，以工程 error.h 为准

[GUARANTEE]
必须实现或修改的接口：

```c
static struct proc_struct *alloc_proc(void);
void proc_init(void);
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags);
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf);
static void copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf);
static int copy_mm(uint32_t clone_flags, struct proc_struct *proc);

你可以根据需要添加：
- 辅助函数（建议设为 static，仅在本模块内部使用，不改变公开接口）
- 模块内部使用的宏定义
- 模块内部的静态变量

[SPECIFICATION]

## alloc_proc

**Pre-Condition**：系统处于可以分配内核内存的初始化或运行阶段，期望得到一个可供后续初始化的 PCB。

**Post-Condition**：
- 成功时返回一个已分配且字段处于安全空白状态的 struct proc_struct *。
- 失败时返回 NULL，且不留下未释放的资源。

**Case 1**：kmalloc() 分配 PCB 成功。
- 将 PCB 的字段初始化为安全空白状态，至少包括：
  - state = PROC_UNINIT
  - pid = -1
  - runs = 0
  - kstack = 0
  - need_resched = 0
  - parent = NULL
  - mm = NULL
  - context 清零
  - tf = NULL
  - pgdir = NULL 或按工程约定设置
  - flags = 0
  - name 清零
  - list_link、hash_link 清零或按工程约定初始化
- 返回 PCB 指针。

**Case 2**：kmalloc() 失败。
- 返回 NULL，不修改任何全局状态。

**Requirements**：
- alloc_proc() 不得把 PCB 留在“部分字段未初始化”的状态，避免后续 do_fork() 或调度代码误用未初始化字段。
- 不得在 alloc_proc() 中分配内核栈、PID 或挂接链表；这些由 do_fork() 等上层函数完成。

## proc_init

**Pre-Condition**：pmm_init()、vmm_init() 等基础初始化已经完成。

**Post-Condition**：
- proc_list、hash_list 等全局结构已初始化。
- idleproc 已创建并成为当前线程，current == idleproc。
- idleproc 的关键字段符合特殊语义：PID 为 0，内核栈复用当前启动栈（如 bootstack），状态为 PROC_RUNNABLE，need_resched 置为 1，parent = NULL，mm = NULL，pgdir 指向内核页表根。
- 通过 kernel_thread() 创建了 initproc，其 PID 为 1，状态为 PROC_RUNNABLE，并已登记到进程链表/哈希表。
- nr_process 正确反映当前已登记线程数。

**Case 1**：idleproc 初始化。
- 直接复用当前执行现场创建 idleproc，而不是通过普通 do_fork()。
- 设置 idleproc->pid = 0、idleproc->kstack = 当前启动栈、idleproc->state = PROC_RUNNABLE、idleproc->need_resched = 1、idleproc->parent = NULL、idleproc->mm = NULL、idleproc->pgdir = boot_pgdir_pa 或工程约定值。
- 设置 current = idleproc，将 idleproc 挂接到 proc_list 和 hash_list。
- 设置进程名，如 "idle"。

**Case 2**：创建 initproc。
- 调用 kernel_thread(init_main, NULL, 0) 或工程约定的初始化函数创建第一个真正内核线程。
- 若创建失败，应按工程约定 panic() 或报告错误，因为系统至少需要一个可运行线程。
- 成功后将 initproc 指向新线程，设置名称如 "init"，并保证其已登记到 proc_list 和 hash_list。

**Requirements**：
- idleproc 不是普通 fork 出来的线程，其 PID、内核栈、调度标记和 current 位置具有特殊含义。
- proc_init() 完成后，idleproc 和 initproc 必须都处于 PROC_RUNNABLE，且 idleproc->need_resched 为 1，以便后续 cpu_idle() 触发第一次调度。
- 全局链表和哈希表的一致性必须保持。

## kernel_thread

**Pre-Condition**：系统已经能够进行内核栈分配和 PCB 分配，希望以函数 fn 和参数 arg 创建一个新的内核线程。

**Post-Condition**：
- 成功时返回新线程的 PID（正整数）。
- 失败时返回负错误码。
- 新线程的首次运行上下文已通过 do_fork() 和 copy_thread() 构造完成。

**Case 1**：正常创建。
- 构造一个临时的 struct trapframe tf，清零后设置：
  - tf.sepc = (uintptr_t)kernel_thread_entry
  - tf.sstatus 按 RISC-V 要求设置：读取当前 sstatus，设置 SPP 和 SPIE，清除 SIE，以表示返回到内核态并保持中断使能状态。
  - 将 fn 放入 tf 中对应被调用函数入口的寄存器（如 s0），将 arg 放入参数寄存器（如 s1）。
- 调用 do_fork(clone_flags, 0, &tf)。
- 返回 do_fork() 的返回值。

**Case 2**：do_fork() 失败。
- 返回 do_fork() 返回的负错误码，不留下临时资源。

**Requirements**：
- kernel_thread() 只负责构造首次运行所需的临时 trapframe 并调用 do_fork()；不直接分配内核栈、不直接挂接链表。
- 设置的 tf.sstatus 必须保证新线程首次通过 sret 返回后处于内核态（S 态），且中断状态符合工程约定。

## do_fork

**Pre-Condition**：
- alloc_proc()、内核栈分配、copy_mm()、copy_thread() 等依赖接口可用。
- 调用者提供 clone_flags、stack 和可选的 tf。

**Post-Condition**：
- 成功时返回新线程/进程的 PID，新 PCB 已初始化、内核栈已分配、上下文和 trapframe 已构造、已挂接到 proc_list 和 hash_list，状态为 PROC_RUNNABLE。
- 失败时返回负错误码，并释放已经占用的资源，不留下半初始化 PCB 或泄漏的内核栈。

按顺序完成：
1. 调用 alloc_proc() 分配并初始化 PCB；失败返回 -E_NO_MEM。
2. 为 PCB 分配内核栈；失败则释放 PCB 并返回 -E_NO_MEM。
3. 调用 copy_mm(clone_flags, proc) 处理地址空间；当前实验阶段 copy_mm() 为占位，通常直接成功。
4. 调用 copy_thread(proc, stack, tf) 设置新线程的 trapframe 和 context。
5. 为新 PCB 分配 PID（如 get_pid()），设置 proc->pid。
6. 将 PCB 挂接到 proc_list 和 hash_list，增加 nr_process。
7. 将新线程状态设为 PROC_RUNNABLE，必要时调用 wakeup_proc()。
8. 返回新线程 PID。

**Requirements**：
- 失败路径必须成对清理：PCB、内核栈、已挂接的链表节点、已分配的 PID 等，不能泄漏。
- 新 PCB 在完全初始化并挂接之前，不得被调度器选中。
- do_fork() 返回的 PID 必须是正整数；失败返回负错误码。
- 当前实验阶段 copy_mm() 不进行真正的用户地址空间复制，因为内核线程的 current->mm 为 NULL，proc->mm 描述的用户地址空间留待后续实验。

## copy_thread

**Pre-Condition**： proc 已分配 PCB 和内核栈；tf 可能是 kernel_thread() 构造的临时 trapframe，也可能是其他调用者提供的 trapframe。

**Post-Condition**：
- 在新线程自己的内核栈上分配并构造了 proc->tf。
- proc->context 已设置，使得 switch_to() 恢复该线程后能通过 forkret -> forkrets -> __trapret 首次运行。
- proc->tf 中的返回值和参数符合“新线程”的语义。

按顺序完成：
- 在新内核栈顶分配 struct trapframe 空间，令 proc->tf 指向它。
- 若传入 tf 非 NULL，将其内容复制到新 trapframe；否则清零初始化。
- 设置新 trapframe 的返回值寄存器（如 a0）为 0，表示子线程/子进程返回值。
- 设置 proc->context.ra = (uintptr_t)forkret。
- 设置 proc->context.sp 指向新 trapframe 或按工程约定指向内核栈顶。
- 保证 trapframe 中的 sepc 和 sstatus 能通过 sret 跳到 kernel_thread_entry 并以 S 态运行。

**Requirements**：
- copy_thread() 必须把 trapframe 放在新线程自己的内核栈上，而不是复用父线程的栈。
- context.ra 必须指向 forkret，context.sp 必须与 trapframe 布局配合，保证 forkret -> forkrets -> __trapret 链路正确。
- 不得直接修改父线程的 trapframe 或 context。

## copy_mm

当前实验阶段不进行真正的用户地址空间复制，proc->mm 保持 NULL，函数返回 0。只为后续实验保留完整函数形态，便于扩展为用户地址空间复制。不得因为 current->mm == NULL 而崩溃或返回错误，应安全地作为占位接口返回成功；不得在 copy_mm() 中分配或复制用户页表。
