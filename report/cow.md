# Challenge 1：实现 Copy on Write （COW）机制

**负责人：** 2413406-黄婉文

COW 是一种延迟复制策略：fork 创建子进程时，不立即复制父进程用户地址空间的物理页，而是让父子进程共享同一批物理页，同时把这些页在双方页表中都标记为只读。只有当某一方真正写某页时，CPU 触发 page fault，内核才在此时复制该页，使写入方获得自己的可写副本，另一方继续持有原页。COW 把复制推迟到真正发生写操作时，节省了内存和时间，同时父子地址空间逻辑上独立，修改互相都不可见。

## 设计说明

实现 COW 机制需要在普通 fork 全量复制的基础上，把可写用户页的复制从 fork 时推迟到写时：fork 时父子共享同一物理页、双方 PTE 清写位并设置 COW 标志、物理页引用计数加一；任一方写该页触发 store page fault，内核判断为 COW 页后分配新页、复制内容、更新写方页表并恢复写权限。只读 vma 直接共享只读页，不设 COW。核心实现包括：

- 定义 PTE 软件 COW 标志（如 PTE_COW），COW 页 PTE 满足 V=1、W=0、COW=1。
- 物理页引用计数表示映射数；fork 共享时 page_ref_inc，COW 复制时旧页 page_ref_dec。
- copy_range 增加 COW 分支：可写 vma 子页表映射同一物理页只读+COW，父 PTE 清写位设 COW，并刷新父 TLB。
- page fault 处理：识别 CAUSE_STORE_PAGEFAULT，检查 vma 可写且 PTE 为 COW；ref==1 直接恢复写权限，ref>1 分配新页复制并替换映射。
- 所有 PTE 修改与引用计数调整在关中断/持锁临界区内完成，并刷新 TLB。

实现中还要注意Dirty COW的问题，其本质是 COW 路径中“判断共享、准备复制”与“重新建立映射”之间存在竞态，导致写入穿透到本应只读的页。在 ucore 的 COW 实现中要避免类似问题：享页在所有进程的 PTE 中都必须先清除 PTE_W，再允许任何一方运行；
do_pgfault 中对 PTE_COW、page_ref 的检查和 PTE 替换必须在同一临界区；复制完成、PTE 更新完成、TLB 失效完成之后，才能返回用户态；不能先把共享页改成可写、再复制，否则其他进程可能在此期间写入。

### 状态转换说明

COW机制下物理页状态可以用类似有限状态自动机的方式说明。以单个物理页及其映射为对象，定义以下状态：

| 状态 | 含义 | PTE 特征 | ref |
|---|---|---|---|
| F | Free，空闲无映射 | 无 | 0 |
| W | Writable，独占可写 | V=1, W=1, COW=0 | 1 |
| C | COW，共享只读，写时复制 | V=1, W=0, COW=1 | ≥1 |

转换条件如下：

| 当前状态 | 事件 | 条件 | 动作 | 下一状态 |
|---|---|---|---|---|
| F | 分配页 | 页面可写 | 建立 PTE，ref=1 | W |
| W | fork | 页面可写 | 父 PTE 清 W、设 COW；子页表映射同页只读并设 COW；ref 加一；刷新父 TLB | C |
| C | fork | 已经是 COW 页 | 子页表映射同页只读并设 COW；ref 加一 | C |
| C | 写缺页 | ref > 1 | 分配新页并复制内容；写方 PTE 指向新页且可写；旧页 ref 减一 | 写方 W；旧页仍为 C |
| C | 写缺页 | ref = 1 | 不复制；当前 PTE 设置 W、清除 COW；刷新 TLB | W |
| C | unmap | ref 减一后 ≥ 1 | 移除映射，ref 减一 | C |
| W 或 C | unmap | ref = 1 | 移除最后一个映射，释放物理页 | F |


## 代码实现

**需要实现/修改的函数：**

```c
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

### 提示词

以下是经过迭代优化后，最终成功实现该功能的提示词：

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

**操作要求**：必须在项目中修改实际文件，而不是仅展示代码。保留现有代码，只在需要实现或补全的位置修改；不要删除既有逻辑，不要引入与当前实验无关的功能。需要完整实现 COW 分支、必要的静态辅助函数和缺页处理逻辑。如果提示词中有内容与代码框架冲突，以现有代码为准。

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


## 测试用例

设计测试用例，覆盖尽可能多的情况，并生成对应接口。以下为最终提示词：

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


## 测试结果