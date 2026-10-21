# AI提示词

## 功能模块一任务一

**2413671-吕旭婧**

以下为实现模块一任务一 page_init() 函数的提示词。

````text
[PROMPT]

我的任务是实现物理内存初始化，为后续分配器提供正确的页描述符和初始空闲页。

请从远端最新 lab2 建立成员 A 的独立开发分支和工作目录。已有合适目录时可以复用。保留当前 lab1 的提交和文件，解压源码作为原始参考，后续以仓库中的 code/ 为开发版本。

在 code/kern/mm/pmm.c 的模块一标记位置实现 static void page_init(void)。保留 lab2 注释，只将本模块的 YOUR CODE 替换为我的学号 2413671。

本轮不实现 Best-Fit、Buddy，不切换当前的 default_pmm_manager，不修改已有测试函数、公共接口、结构体和地址宏。

[RELY]

请以指导书和当前工程定义为准，使用已有的：

- get_memory_base()、get_memory_size()；
- struct Page、pages、npage、nbase、va_pa_offset；
- 内存上界、映射偏移、页大小、地址转换和对齐宏；
- 链接符号 end、页面状态宏、init_memmap()；
- panic()、cprintf()。

dtb_init() 在 pmm_init() 之前执行。npage 表示物理页号上界，pages 是描述符数组的虚拟地址，每个描述符对应的物理页号从 nbase 开始。

[GUARANTEE]

交付可编译的 page_init() 实现。成功初始化后，内核能够查询物理内存布局，并使用现有 First-Fit 分配和回收页面。

只修改本模块所需的位置。若发现必须修改范围以外的文件，请说明原因，不要顺带修补其他成员的任务。

[SPECIFICATION]

1. 设备树内存信息必须有效。零大小、地址范围计算溢出，以及与本工程固定映射假设不兼容的范围，不能继续用于初始化，应给出明确的错误信息并终止。

2. va_pa_offset 与工程已有映射关系一致。可管理范围不能超过设备树报告的实际内存，也不能超过工程规定的物理内存上界，不把默认 128 MiB 当作探测结果写死。

3. pages 位于内核 end 之后并按页对齐，数组覆盖 [nbase, npage) 对应的物理页。应确认数组能够完整放入有效内存，避免在发现越界前就写入数组。

4. 尚未交给分配器的页面应保持保留状态。OpenSBI、内核镜像、描述符数组及相关对齐占用空间，不能进入空闲区。

5. 只把描述符数组之后、有效内存范围内的完整页面交给 init_memmap()。不足一页的尾部空间不能加入；没有完整空闲页时，不调用 init_memmap()。

6. 正确区分虚拟地址、物理地址、物理页号和描述符地址，保证地址转换不会产生越界描述符。

7. 输出探测到的物理内存范围，便于核对初始化结果。

````


## 功能模块二任务二

**2413396-范珂祎**

以下为实现 Best-Fit 分配器的提示词。

````text
[PROMPT]

我的任务是实现 Best-Fit 连续物理内存分配算法，并让系统切换到该分配器。

请基于当前仓库的 code/ 目录开发。保留 lab2 注释，在 code/kern/mm/best_fit_pmm.c 的模块二任务二标记位置实现 Best-Fit 分配器，并将标记改为我的学号 2413396。

完成实现后，在 code/kern/mm/pmm.c 的 init_pmm_manager() 中把 pmm_manager 切换为 best_fit_pmm_manager。

本轮不实现 Buddy、SLUB，不修改已有的 basic_check()、best_fit_check() 等测试函数、公共接口、结构体和地址宏。暂不推送 GitHub。

[RELY]

请以指导书和当前工程定义为准，使用已有的：

- struct pmm_manager、struct Page、free_area_t；
- free_list、nr_free、le2page 及 list_init、list_add、list_add_before、list_del、list_next、list_prev、list_empty 等链表操作；
- PageReserved、SetPageProperty、ClearPageProperty、PageProperty、set_page_ref 等页面状态宏；
- code/kern/mm/default_pmm.c 中的 First-Fit 实现作为参考；
- pmm.c 中的 init_pmm_manager() 用于切换分配器。

Best-Fit 与 First-Fit 的区别只在 alloc_pages 的选择策略；初始化、登记空闲区、回收合并和空闲页计数沿用 First-Fit 的结构。

[GUARANTEE]

交付可编译的 best_fit_pmm.c 实现，并完成分配器切换。成功初始化后，内核显示 memory management: best_fit_pmm_manager，best_fit_check() 通过并输出 check_alloc_page() succeeded!。

只修改本模块所需的位置。若发现必须修改范围以外的文件，请说明原因，不要顺带修补其他成员的任务。

[SPECIFICATION]

1. best_fit_init() 将空闲链表初始化为空，nr_free 置 0。

2. best_fit_init_memmap(base, n) 登记一段连续空闲区：逐页断言保留状态后清空 flags/property/ref，头页 property 记为 n 并置 PG_property，按物理地址升序插入空闲链表，nr_free += n。

3. best_fit_alloc_pages(n) 分配 n 个连续页：
   - n 非法或 n > nr_free 时返回 NULL；
   - 遍历空闲链表，选择 property >= n 且 property 最小的空闲块（Best-Fit 的核心）；
   - 若块大小恰等于 n，整块从链表删除；若大于 n，从低地址端切出 n 页，剩余 property - n 页在 page + n 处成为新块，插回原位置保持地址升序；
   - nr_free -= n，返回头页并清除其 PG_property。

4. best_fit_free_pages(base, n) 回收 n 个连续页：逐页断言非保留且非块头后清空标志，头页 property 记为 n 并置 PG_property，按地址升序插入，nr_free += n；随后先与低地址相邻块合并，再与高地址相邻块合并。

5. best_fit_nr_free_pages() 返回当前 nr_free。

6. best_fit_pmm_manager 的字段齐全，name 为 best_fit_pmm_manager。

7. 不改动 basic_check() 与 best_fit_check()，最终 best_fit_check() 通过。best_fit_check() 中的断言 p0 + 4 == p1 用于区分 Best-Fit 与 First-Fit，应原样通过。
````


## 功能模块二任务三

**2413406-黄婉文**

### Prompt 1：Buddy System 代码实现

````markdown

[PROMPT]
**任务**：参考现有 ucore 物理内存管理框架，从零生成 Buddy System 分配模块并接入内核。在 kern/mm 文件夹下新增buddy_system_pmm.h、buddy_system_pmm.c文件，实现 struct pmm_manager要求的完整接口；定义buddy_system_pmm_manager，在kern/mm/pmm.c的init_pmm_manager()中将当前物理内存管理器切换为buddy_system_pmm_manager；同时在 Makefile中把buddy_system_pmm.o加入编译。

**操作要求**：你必须在项目中修改和新增实际文件，生成可运行的代码，完成伙伴系统内存管理框架的实现。不要修改 default_pmm.c、best_fit_pmm.c以及struct pmm_manager的接口定义，包括page_init()、alloc_pages()、free_pages()、nr_free_pages()、check_alloc_page()等函数的转发逻辑。

**输出要求**：直接进行文件操作，保留文件中的现有代码，实现或修改的代码应插入或更新到相应的位置，不要引入无关功能和不必要的函数，除非必需的辅助函数。使用下面[RELY]、[GUARANTEE]、[SPECIFICATION]部分的信息。


[RELY]
以下为完成任务可能所需的上下文信息，包括宏定义、数据结构、可调用的函数、相关全局变量等，包括但不限于：

// 宏定义
mmu.h: PGSIZE, PGSHIFT
riscv.h: DRAM_BASE
memlayout.h: PG_reserved, PG_property
pmm.h: alloc_page(), free_page(page) 

// 数据结构
memlayout.h: struct Page {
    int ref;
    uint64_t flags;
    unsigned int property;
    list_entry_t page_link;
};
// struct Page 每个实例描述一个 4KB 物理页；property 在 Buddy 中记录该块分配时的 order；page_link 用于挂空闲链表
memlayout.h: free_area_t {
    list_entry_t free_list;
    unsigned int nr_free;
};
list.h: list_entry_t
pmm.h: struct pmm_manager

// 可用函数
memlayout.h: SetPageProperty, ClearPageProperty, PageProperty, le2page
list.h: list_init, list_empty, list_add, list_add_before, list_del, list_next, list_prev
pmm.h: page2ppn(page), page2pa(page), pa2page(pa)

// 全局变量
pmm.h: pages, npage, nbase
// pages 指向 Page 描述符数组首地址，数组覆盖物理页号 [nbase, npage)
// nbase = DRAM_BASE / PGSIZE，pages[0] 对应物理页号 nbase，即物理地址 DRAM_BASE
// pages[i] 对应物理页号 nbase + i，对应物理地址 (nbase + i) << PGSHIFT
page2ppn(page) = page - pages + nbase
page2pa(page) = page2ppn(page) << PGSHIFT
pa2page(pa) = &pages[PPN(pa) - nbase]
// 由 Page 指针求物理页号必须用 page2ppn，求物理地址必须用 page2pa，不能用 page - pages 直接当物理页号


[GUARANTEE]
必须新增的接口如下：
定义 buddy_system_pmm_manager，并在头文件中声明；

kern/mm/buddy_system_pmm.c 必须实现以下静态函数：
static void buddy_init(void);
static void buddy_init_memmap(struct Page *base, size_t n);
static struct Page *buddy_alloc_pages(size_t n);
static void buddy_free_pages(struct Page *base, size_t n);
static size_t buddy_nr_free_pages(void);

buddy_init：初始化 Buddy 管理器的内部状态，包括所有 order 的空闲链表和总空闲页计数。

buddy_init_memmap：把从 base 开始的连续 n 个物理页登记为 Buddy 空闲块；清除这些页的 PG_reserved，设置首页 PG_property，按 order 对齐要求把这段页拆成若干 2 的幂大小的块，分别挂到对应 order 的空闲链表，并更新总空闲页计数。n 不保证是 2 的幂，base 的页号不保证按 n 对齐。

buddy_alloc_pages：分配至少 n 页连续物理页，返回块首页的 struct Page *；实际分配块大小是大于等于 n 的最小 2 的幂；成功时从对应 order 空闲链表摘除，清除该页 flags 中的 PG_property 标记，并在 property 字段记录实际分配的 order，更新总空闲页计数；失败时返回 NULL，不修改任何链表和计数。

buddy_free_pages：释放由 buddy_alloc_pages 返回的块首页 base；以 base->property 记录的 order 为准，把块挂回对应 order 空闲链表；如果伙伴块也空闲且同 order，则合并并递归向上合并；合并后更新首页 PG_property 和 property，摘除被合并页的链表节点，更新总空闲页计数。

buddy_nr_free_pages：返回当前所有 order 空闲块包含的页数总和。

必须修改的现有接口：
kern/mm/pmm.c 中的 init_pmm_manager()：把 pmm_manager 指向 buddy_system_pmm_manager，调用 pmm_manager->init()，输出管理器名称；保留原有 First-Fit / Best-Fit 管理器定义，保持可以通过修改一行指针切换内存管理器。

Makefile：把 buddy_system_pmm.o 加入编译，使新增模块参与链接。

你可以根据需要添加：
- 辅助函数（建议设为static，只在本模块内部使用，不应该改变公开接口）
- 内部使用的宏定义
- 模块内部的静态变量

[SPECIFICATION]
以下为每个函数的行为规格：
## buddy_init

**Pre-Condition**: 由init_pmm_manager()调用，此时还没有登记空闲块。

**Post-Condition**: Buddy 管理器的内部空闲结构与计数处于空状态，所有 order 对应的空闲链表都为空，总空闲页计数为 0。

## buddy_init_memmap

**Pre-Condition**: base指向连续物理页的首页描述符，n > 0；这段页处于保留状态，尚未登记为空闲；n不保证是 2 的幂，base页号不保证按某个 2 的幂对齐。

**Post-Condition**: 每个描述符引用计数为 0，保留标记被清除；这段页被拆成若干 2 的幂大小的块，每块首页标记为空闲块首页并记录其 order；每块首页页号按其 order 对齐，块内非首页不标记；所有块首页挂到对应 order 的空闲链表，总空闲页计数增加 n。

**Case 1：n是2的幂且base页号按n对齐**
- 整段作为单个 order 为对应幂次的块登记。

**Case 2：n是 2 的幂但base 页号不按n对齐，或n不是2的幂**
- 按对齐要求和2的幂大小拆成多个块，分别挂到对应 order。

**Case 3：n == 0**
- 不登记任何块，计数不变。

## buddy_alloc_pages

**Pre-Condition**: 管理器已初始化，init_memmap已登记空闲页。

**Post-Condition**: 成功时返回块首页，实际块大小是大于等于n的最小 2 的幂；返回块已从对应 order 链表摘除，不再是空闲块首页，并记录分配时的 order；总空闲页计数减去实际块大小。失败时返回空指针，链表和计数不变。

**Case 1：目标 order 有空闲块**
- 直接摘下返回。

**Case 2：目标 order 无空闲块，但更大 order 有空闲块**
- 从更大 order 逐级分裂，另一半作为对应 order 的空闲块登记，直到得到目标 order。

**Case 3：空闲页不足或所有更大 order 都无可用块**
- 返回空指针，不修改链表和计数。

## buddy_free_pages

**Pre-Condition**: base块当前不在空闲链表中，且首页记录了分配时的 order。

**Post-Condition**: 该块被标记为空闲块首页并挂回对应 order 链表；伙伴空闲且同 order 时合并，并递归向上合并；合并后首页标记为空闲块首页并记录新 order，被合并的首页不再是空闲块首页并从链表摘除，总空闲页计数增加实际释放块大小。

**Case 1：伙伴不空闲或不在空闲链表**
- 直接把该块挂回对应 order 链表，计数增加该块大小。

**Case 2：伙伴空闲且同 order**
- 合并为 order+1 的块，首页取页号较小者，继续向上检查，直到伙伴不空闲、超出最大 order 或伙伴页号越界。

## buddy_nr_free_pages

**Pre-Condition**: 管理器已初始化。

**Post-Condition**: 返回所有 order 空闲块包含的页数总和，与遍历链表累加结果一致。

注：以上行为描述可能有错漏，可以根据实际情况进行适当调整。

````


### Prompt 2：实现buddy_check()测试函数

````markdown

[PROMPT]
**任务：** 在 kern/mm/buddy_system_pmm.c 中实现buddy_check()函数，设计并实现完整的 Buddy System 测试用例，验证 buddy_init_memmap、buddy_alloc_pages、buddy_free_pages、buddy_nr_free_pages等函数的行为是否正确。测试失败时通过 assert 终止；测试通过时尽量恢复测试前的空闲状态。

**操作要求：** 只在 kern/mm/buddy_system_pmm.c 中实现 buddy_check() 及必要的 static 辅助测试函数，不要修改相关函数的实现；不要修改其他内存管理器的代码，包括default_pmm.c、best_fit_pmm.c、pmm.h、pmm.c 的转发逻辑；可以参考default_check 或 best_fit_check。

**输出要求：** 直接进行文件操作，保留现有代码，只新增 buddy_check() 及 static 辅助测试函数，不要引入无关功能。


[RELY]
// 宏定义
mmu.h: PGSIZE, PGSHIFT
riscv.h: DRAM_BASE
memlayout.h: PG_reserved, PG_property
pmm.h: alloc_page(), alloc_pages(n), free_page(page), free_pages(base, n), nr_free_pages()
defs.h: NULL、size_t、uintptr_t、ppn_t、ROUNDUP、ROUNDDOWN
// 注：defs.h 中 bool 的定义是 typedef int bool，没有 true / false，也没有 <stdbool.h>

// 数据结构
memlayout.h: struct Page {
int ref;
uint64_t flags;
unsigned int property;
list_entry_t page_link;
};
// property 在 Buddy 中记录该块分配时的 order；page_link 用于挂空闲链表
memlayout.h: free_area_t {
list_entry_t free_list;
unsigned int nr_free;
};
list.h: list_entry_t
pmm.h: struct pmm_manager

// 可用函数
memlayout.h: PageProperty, le2page
list.h: list_init, list_empty, list_add, list_add_before, list_del, list_next, list_prev
pmm.h: page2ppn(page), page2pa(page), pa2page(pa)
pmm.h: alloc_page(), alloc_pages(n), free_page(page), free_pages(base, n), nr_free_pages()
stdio.h: cprintf

// 全局变量
pmm.h: pages, npage, nbase
// page2ppn(page) = page - pages + nbase
// 伙伴页号按 pfn ^ (1 << order) 计算
// 伙伴 Page 指针 = &pages[buddy_pfn - nbase]

// 测试约束
buddy_check 由 pmm_manager->check() 调用，测试中只能通过 alloc_pages / free_pages / nr_free_pages 访问分配接口，不要直接调用 buddy_alloc_pages / buddy_free_pages


[GUARANTEE]
必须实现的接口：
static void buddy_check(void);

允许新增：
- static 辅助测试函数，只在本模块内部使用；
- 测试辅助宏；
- 测试辅助静态变量。


[SPECIFICATION]

buddy_check：

**Pre-Condition: **
Buddy 管理器已初始化，init_memmap 已登记空闲页；当前 pmm_manager 指向 buddy_system_pmm_manager。

**Post-Condition: **
执行完整的 Buddy 测试用例；所有测试通过时，管理器空闲状态尽可能恢复到测试前；任一测试失败时通过 assert 终止。

测试必须覆盖包括但不限于以下情况：

**Case 1：初始状态一致性**

遍历所有 order 空闲链表，检查每个节点首页 PageProperty 为真，property 等于该节点所在 order；每个节点首页页号按其 order 对齐；所有 order 链表累加的块大小等于 nr_free_pages()。

**Case 2：基本分配请求**

分配2的幂次页，检查返回块首页 property 对应的 order；释放后检查 nr_free_pages() 恢复。

**Case 3：非 2 的幂分配请求**

分配非2的幂次页，检查返回块首页 property 对应的 order；释放后检查nr_free_pages()恢复。

**Case 4：分裂**

在目标 order 无空闲块时，从更大 order 分裂；检查分裂后出现两个同 order 的伙伴块，其中一个是空闲块，且该空闲块首页按其 order 对齐。

**Case 5：伙伴合并**

分配两个互为伙伴的块，分别释放；检查第二次释放后合并成更大的块，nr_free_pages() 正确。

**Case 6：递归合并**

构造连续多个 order 的伙伴块，按顺序释放；检查能递归向上合并到最大可能块，链表节点数量减少。

**Case 7：空闲不足**

请求超过当前空闲页数的分配，返回 NULL；检查 nr_free_pages() 和链表未被破坏。

**Case 8：计数一致性**

在多次分配释放后，遍历所有 order 链表累加结果与 nr_free_pages() 一致。

````


## 功能模块二challenge

**2413406-黄婉文**

### Prompt 1：slub算法代码实现

````markdown

[PROMPT]
**任务：** 参考现有 ucore 物理内存管理框架，从零生成简化版 SLUB 内存对象分配模块并接入内核。在 kern/mm 文件夹下新增 slub.h、slub.c 文件，实现对象缓存层：向当前 pmm_manager 申请整页作为 slab，把 slab 切分成固定大小的对象，用空闲对象内部的前几个字节存放 freelist 指针，对外提供 slub_alloc / slub_free 接口。Makefile 中把 slub.o 加入编译，使新增模块参与链接。

**操作要求：** 你必须在项目中修改和新增实际文件，生成可运行的代码，完成 SLUB 对象缓存层的实现。不要修改 default_pmm.c、best_fit_pmm.c、buddy_system_pmm.c 以及 struct pmm_manager 的接口定义，包括 page_init()、alloc_pages()、free_pages()、nr_free_pages()、check_alloc_page() 等函数的转发逻辑。

**输出要求：** 直接进行文件操作，保留文件中的现有代码，实现或修改的代码应插入或更新到相应的位置，不要引入无关功能和不必要的函数，除非必需的辅助函数。使用下面 [RELY]、[GUARANTEE]、[SPECIFICATION] 部分的信息。

[RELY]
以下为完成任务可能所需的上下文信息，包括宏定义、数据结构、可调用的函数、相关全局变量等，包括但不限于：

// 宏定义
mmu.h: PGSIZE, PGSHIFT
memlayout.h: PG_reserved, PG_property
pmm.h: alloc_page(), free_page(page)

// 数据结构
memlayout.h: struct Page {
int ref;
uint64_t flags;
unsigned int property;
list_entry_t page_link;
};
list.h: list_entry_t
pmm.h: struct pmm_manager

// 可用函数
pmm.h: alloc_pages(n), free_pages(base, n), nr_free_pages()
pmm.h: page2ppn(page), page2pa(page), pa2page(pa)

// 全局变量
pmm.h: pages, npage, nbase, pmm_manager
// alloc_pages(n) 返回 struct Page *，即 slab 首页描述符
// page2pa(page) 给出物理地址
// nbase = DRAM_BASE / PGSIZE，pages[0] 对应物理页号 nbase

// 注：slub 只调用 alloc_pages / free_pages，不直接操作 Page 的 flags 和 property 字段


[GUARANTEE]
要新增的接口如下：

kern/mm/slub.h 必须声明：
void slub_init(void);
void *slub_alloc(size_t size);
void slub_free(void *ptr);

kern/mm/slub.c 必须实现：
void slub_init(void);
void *slub_alloc(size_t size);
void slub_free(void *ptr);

定义size class 数组和 slub_cache 结构体，覆盖 16、32、64、128、256、512、1024、2048、4096 字节，一个size class对应一组 slub_cache；最大支持对象大小 4096 字节，超过返回 NULL。

slub_init：初始化所有 size class 的 cache，包括 object_size、total_size、objects_per_slab 和 freelist，不向 pmm_manager 申请任何页。

slub_alloc：按 size 找到对应 cache，从 freelist 头部摘取一个对象返回；freelist 为空时向 alloc_pages(1) 申请一页，切分为 objects_per_slab 个对象串成 freelist，再摘取一个返回；分配失败返回 NULL。

slub_free：由 ptr 恢复出所属 cache 和对象起始地址，把对象头插回 freelist；ptr 为 NULL 时不做任何操作。

要修改的现有接口：
Makefile 中把 slub.o 加入编译，使新增模块参与链接。

你可以根据需要添加：

- 辅助函数（建议设为 static，只在本模块内部使用，不应该改变公开接口）
- 内部使用的宏定义
- 模块内部的静态变量


[SPECIFICATION]
以下为每个函数的行为规格：

struct slub_cache 与 size class：
**Pre-Condition: ** 模块内部有一个 size class 数组，覆盖 16 到 4096 字节的 2 的幂。

**Post-Condition: ** 每个 size class 对应一个 slub_cache；object_size 等于该 size class 的用户数据区大小；total_size 包含对象管理元数据和用户数据区并按 8 字节对齐；objects_per_slab 等于 PGSIZE 除以 total_size 向下取整且至少为 1；freelist 初始为空。

slub_init：
**Pre-Condition: ** pmm_manager 已初始化，alloc_pages / free_pages 可用。

**Post-Condition: ** 所有 size class 的 cache 被初始化，freelist 为空，尚未向 pmm_manager 申请页。

slub_alloc：
**Pre-Condition: ** size > 0，size 不超过最大 size class。

**Post-Condition: ** 成功时返回至少 size 字节的对象地址，返回对象已从对应 cache 的 freelist 摘除；失败时返回 NULL。

**Case 1：对应 cache 的 freelist 非空**

从 freelist 头部摘下对象，返回用户数据区地址。

**Case 2：对应 cache 的 freelist 为空，且 alloc_pages(1) 成功**

把新 slab 切分为 objects_per_slab 个对象，串成 freelist，再从中摘取一个返回。

**Case 3：对应 cache 的 freelist 为空，且 alloc_pages(1) 失败**

返回 NULL，不修改 freelist，不增加任何计数。

**Case 4：size 超过最大 size class**

返回 NULL。

slub_free：
**Pre-Condition: ** ptr 是之前由 slub_alloc 返回的地址，ptr 非空。

**Post-Condition: ** 对象被归还到其所属 cache 的 freelist 头部；页级空闲计数不变；不产生重复节点，不产生跨 cache 的错挂。

**Case 1：正常释放**

从 ptr 恢复出对象起始地址和所属 cache，把对象头插回 freelist。

**Case 2：ptr 为 NULL**

不做任何操作。

````

### Prompt 2：实现测试函数

````markdown

[PROMPT]
**任务：** 完善 kern/mm/slub.c 中的 slub_check() 函数，设计并实现完整的 SLUB 测试用例，验证 slub_init、slub_alloc、slub_free 在不同大小对象上的分配和释放是否正确。测试失败时通过 assert 终止；测试通过时尽量释放已分配对象，恢复测试前的空闲状态。在 kern/init/init.c 的 kern_init() 中 pmm_init() 之后增加一行 slub_check() 调用，用于触发 SLUB 模块，并保证 make qemu 时能看到 slub_check 的输出。

**操作要求：** 在 kern/mm/slub.c 中实现 slub_check() 及必要的 static 辅助测试函数，每种情况的测试都要有断言；不要修改 slub_init、slub_alloc、slub_free 及 slub_new_slab 的实现，不要修改 default_pmm.c、best_fit_pmm.c、buddy_system_pmm.c、pmm.h、pmm.c 的转发逻辑。可以阅读 default_check 或 best_fit_check 了解测试写法，但 slub_check 必须自己设计用例，不得直接复用。

**输出要求：** 直接进行文件操作，保留现有代码，只新增 slub_check() 及static辅助测试函数，不要引入无关功能。使用下面 [RELY]、[GUARANTEE]、[SPECIFICATION] 部分的信息。

[RELY]
以下为完成任务可能所需的上下文信息：

// 宏定义
mmu.h: PGSIZE, PGSHIFT
pmm.h: alloc_page(), free_page(page)

// 数据结构
kern/mm/slub.h: struct slub_cache {
    size_t object_size;
    size_t total_size;
    size_t objects_per_slab;
    void *freelist;
};

// 可用函数
kern/mm/slub.h: slub_init(), slub_alloc(size), slub_free(ptr)
pmm.h: nr_free_pages()
stdio.h: cprintf

// 测试约束
slub_check 中只能通过 slub_alloc / slub_free 访问对象分配接口，通过 nr_free_pages() 观察页级空闲变化
不要直接调用 alloc_pages / free_pages


[GUARANTEE]
必须实现或完善的接口：
void slub_check(void);

允许新增：
- static 辅助测试函数，只在本模块内部使用；
- 测试辅助宏；
- 测试辅助静态变量。


[SPECIFICATION]

## slub_check

**Pre-Condition:**
当前 pmm_manager 可用，nr_free_pages() 可调用；slub 模块可能已初始化或未初始化。

**Post-Condition:**
先调用 slub_init 确保模块已初始化；执行一组对象级分配释放测试，覆盖不同 size class；所有测试通过时，尽量释放所有已分配对象，恢复 slub 的空闲状态；任一测试失败时通过 assert 终止。

测试必须覆盖包括但不限于以下情况：

**Case 1：所有 size class 分配释放**
- 对 16、32、64、128、256、512、1024、2048、4096 每个 size class 分别分配一个对象，检查返回非空、地址按 8 字节对齐；
- 对每个对象写入特征值再读回，检查写入成功；
- 释放后再次分配同 size class 对象，检查能复用；
- 全部释放后检查页级空闲计数与测试前一致。

**Case 2：跨 size class 隔离**
- 连续分配 16、64、256、1024、4096 字节对象各一个，检查返回非空、地址互不重叠；
- 对每个对象写入不同特征值再读回，检查互不覆盖；
- 释放其中一个，检查其他对象内容不变。

**Case 3：freelist 耗尽触发新 slab**
- 对 32 字节 size class 连续分配超过 objects_per_slab 个对象，检查返回非空；
- 检查页级空闲计数相应减少，说明 slub 向 pmm_manager 申请了新的页；
- 全部释放后检查页级空闲计数恢复。

**Case 4：边界情况**
- 请求 0 字节返回 NULL；
- 请求 8192 字节返回 NULL；
- slub_free(NULL) 不崩溃、不改变 freelist 和页级计数。

````