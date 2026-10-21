# 操作系统实验报告

## 实验基本信息

| 项目 | 内容 |
|------|------|
| **实验名称** | Lab 2: 物理内存和页表 |
| **小组成员** | 2413396-范珂祎、2413406-黄婉文、2413671-吕旭婧 |
| **完成日期** | 2026-10-21 |


### 小组分工

实验任务分工：

| 成员 | 负责的练习/模块 |
|------|----------------|
| 2413396-范珂祎 | 功能模块二任务一（First-Fit 理解）、任务二（Best-Fit 实现） |
| 2413406-黄婉文 | 功能模块二任务三，模块二扩展任务challenge，分支任务gdb调试 |
| 2413671-吕旭婧 | 模块一物理内存初始化以及后续报告整合 |

实验报告分工：

| 成员 | 负责的部分 |
|------|----------------|
| 2413396-范珂祎 | 模块二任务一，任务二的说明、规格与提示词、验证 |
| 2413406-黄婉文 | 实验目的，模块二任务三、扩展任务，分支任务，实验报告修改 |
| 2413671-吕旭婧 | 模块一说明、规格与提示词、验证、模块一 Challenge，材料整合 |

---

## 一、实验目的

<!-- 说明本次实验的主要目标和预期成果 -->

本实验的主要目的是：

1. 目的1：理解页表的建立和使用方法
2. 目的2：理解物理内存的管理方法
3. 目的3：理解页面分配算法

---

## 二、实验环境

你们使用的 AI 工具

| 成员 | AI 编程工具 | 底层模型 | 备注 |
|------|------------|---------|------|
| 2413396-范珂祎 | Claude Code（终端 Agent） | Claude Fable 5 | |
| 2413406-黄婉文 | DeepSeek Harness | DeepSeek-V4.1-Flash | |
| 2413671-吕旭婧 | Codex Desktop | GPT-6 | |

**说明：**
- **AI 编程工具**：指具体使用的终端工具、编辑器插件、桌面应用或浏览器界面
- **底层模型**：指该工具使用的大语言模型及版本

---

## 三、实验整体逻辑分析

### 3.1 本章节的逻辑主线

本实验围绕物理内存的探测、组织和分配管理展开。entry.S 里的启动页表先把内核映射到高虚拟地址，进入 kern_init() 后，pmm_init() 先选择并初始化分配器，再由 page_init() 建立页描述符布局并排除已用空间，最后通过 init_memmap() 把空闲页注册给当前分配器。本实验涉及的页分配器包括框架自带的 First-Fit、任务二实现的 Best-Fit、任务三实现的 Buddy System，三者共用同一套 struct pmm_manager 接口。

### 3.2 功能的逐步实现

1. 实现page_init()。内核首先要从设备树探测物理内存大小，再排除已经被OpenSBI、内核镜像和管理数据占用的部分，为空闲物理页建立描述信息，最后再把这些空闲页交给当前 pmm_manager 管理。
2. 理解First-Fit算法；实现Best-Fit的完整功能，将First-Fit算法切换为Best-Fit算法，运行qemu进行验证。
3. 参考现有物理内存管理框架实现Buddy System，设计相应测试函数并编译运行；另外实现slub分配算法并测试验证。
4. 使用gdb调试页表查询过程，理解地址转换和页表查询的完整流程。

---

## 四、实验内容与实现

<!-- 
说明：本部分按照 exercises 文档中的练习顺序组织
每个功能模块或者练习中可能包含多项任务，请根据实际情况调整
-->

### 功能模块一：物理内存初始化功能

**负责人：** 2413671-吕旭婧

#### 模块功能描述

**需要实现/修改的函数：**

```c
static void page_init(void);
```

**功能说明：**

page_init() 负责读取设备树接口保存的内存信息，安排物理页描述符的存放位置，排除包括OpenSBI、内核镜像、描述符等不能分配的区域，进行页对齐后再把空闲页交给当前分配器。调用关系为：

```text
kern_init() → dtb_init() → pmm_init()
                             ├─ init_pmm_manager()
                             ├─ page_init() → init_memmap()
                             └─ check_alloc_page()
```

框架的 dtb_init() 先解析内存节点并保存起点、大小，page_init() 再通过 get_memory_base()、get_memory_size() 读取结果。

描述符数量为 npage - nbase，其中npage为可管理物理页号的上界，nbase为内存起点对应的物理页号。pages 为描述符数组的虚拟地址，pages[i] 描述物理页号 nbase + i 的页面，描述符所在的物理地址可以通过 PADDR(pages) 获取。另外注意page2pa(pages) 得到的是第一个描述符所描述的物理页地址，二者不能混用。

#### 核心实现说明

1. 检查起点、大小以及范围加法。计算末端前先判断溢出，避免先溢出再使用错误结果。
2. 使用工程已有偏移设置 va_pa_offset。可管理末端受实际 RAM 和0x88000000上界共同限制，因此 256 MiB RAM 并不意味着本工程可以分配全部 256 MiB。
3. 将内核末端向上按页对齐，作为描述符数组的起点。数组大小为 `(npage - nbase) * sizeof(struct Page)`，在整体容量检查通过后才写入数组。
4. 描述符数组本身也存放在 RAM 中，不能分配，因此 OpenSBI、内核镜像、描述符数组及相关对齐空间均保持保留状态。
5. 把数组末端向上对齐，作为候选空闲区起点。只有它低于可管理末端时才调用 init_memmap()；末尾不足一页的空间不加入空闲区。

init_memmap() 是分配器接口的包装函数，会转交给当前 `pmm_manager->init_memmap()`。本轮使用原有 First-Fit，由它建立空闲链表并更新空闲页数量。后续切换其他分配器仍可复用模块一提供的初始化结果。

#### 最终提示词

本模块使用一版实现提示词，没有代码修复迭代。

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

#### 实现迭代过程

实际过程为一次实现、两轮验证：

1. **工程检查。** 确认设备树接口、固定映射和已有测试。发现原始 Best-Fit 骨架尚未完成，但默认构建会把它一起编译，因此需要隔离该编译单元。
2. **实现与首次验证。** 完善 `page_init()`，使用命令行构建参数临时排除 `best_fit_pmm.c`，成功通过已有 First-Fit 检查。
3. **专项验证。** 在独立副本中接入原始 `test_page_init()`，再对不同 RAM 大小、无效接口值和空间边界做 13 个场景的检查。本轮场景通过，未发现需要修改正式实现的问题。

---

### 扩展任务 Challenge：硬件的可用物理内存范围的获取方法（思考题）

**负责人：** 2413671-吕旭婧

OS 可以通过启动时由固件或引导程序提供的硬件描述，获取内存布局，而不必提前知道机器安装了多少 RAM。

本实验采用设备树。QEMU 准备 DTB，启动链通过参数传入其物理地址，框架的 `dtb_init()` 解析内存节点的 `reg` 属性并保存起点、大小。`page_init()` 使用这两个接口值，进一步排除已占用区域。设备树规范中的内存节点用于描述物理内存布局，`reg` 中包含地址和长度；这也解释了为什么改变 QEMU 的 RAM 大小时不应修改初始化算法。[设备树规范：内存节点](https://devicetree-specification.readthedocs.io/en/v0.3/devicenodes.html)

其他平台也可以由固件或引导程序传递内存布局。例如，UEFI 平台可通过 `GetMemoryMap()` 将内存资源信息提供给 OS 加载器，再由启动过程交给内核。[UEFI GetMemoryMap 接口说明](https://uefi.org/htmlspecs/ACPI_Spec_6_4_html/15_System_Address_Map_Interfaces/uefi-getmemorymap-boot-services-function.html)

获取到 RAM 范围不等于其中每一页都可分配。内核还需要识别保留区和已占用区，检查范围有效性，并按页组织剩余内存。本实验采用固定 DRAM 基址和单段内存的简化框架，不等同于实现了通用平台的全部内存探测方法。

---


### 功能模块二：物理内存分配功能

### 任务一：理解first-fit 连续物理内存分配算法

**负责人：** 2413396-范珂祎

本任务是理解题，不涉及代码修改与提示词生成。通过阅读 [code/kern/mm/default_pmm.c](../code/kern/mm/default_pmm.c)，理解 First-Fit 分配器如何初始化空闲页、分配连续物理页、回收页面并合并相邻空闲块。

#### 模块功能描述

First-Fit 用一条按物理地址升序排列的双向链表 `free_list` 组织所有空闲块，并用 `nr_free` 记录空闲页总数。每个空闲块由地址最低的 `Page`（头页）代表：头页的 `PG_property` 标志位为 1，`property` 字段记录本块包含的页数；块内其余页的 `property` 恒为 0。链表按地址升序，是为了让回收时能在 O(1) 内定位相邻块进行合并。

#### 核心函数说明

**`default_init`：初始化分配器状态。**
`list_init(&free_list)` 将空闲链表初始化为空，`nr_free` 置 0。它由 `init_pmm_manager()` 在 `page_init()` 之前调用，让分配器处于「没有任何空闲页」的初始状态。

**`default_init_memmap`：登记一段连续空闲区。**
`page_init()` 建立页描述符后，把描述符数组之后的连续空闲页交给 `init_memmap(base, n)`。它逐页断言这些页已被标记为保留（这是 `page_init` 设置的），然后清空 `flags`、`property` 与 `ref`；只把头页的 `property` 设为 `n` 并打上 `PG_property`，其余页保持 `property = 0`；最后按地址升序插入 `free_list`，`nr_free += n`。

**`default_alloc_pages`：First-Fit 分配。**
这是「first」的体现。它先判断 `n > nr_free` 则返回 NULL；否则从头遍历 `free_list`，取第一个 `property >= n` 的空闲块，不比较哪个更合适。若块大小正好等于 `n`，整块从链表删除；若块比需求大，把低地址端 `n` 页分配出去，剩余 `property - n` 页在 `page + n` 处成为新块，插回原位置保持地址升序。最后 `nr_free -= n`、`ClearPageProperty(返回头页)`。

**`default_free_pages`：回收并合并相邻空闲块。**
它逐页断言「非保留、非空闲块头」后清空标志，把头页 `property` 设为 `n` 并插入链表，`nr_free += n`。随后先与低地址邻块合并（若 `p + p->property == base`，说明两段物理地址相邻），再与高地址邻块合并（若 `base + base->property == p`），被并进的头页 `ClearPageProperty` 并从链表删除。用指针差判断相邻利用了 `Page` 数组连续分配这一事实。

#### First-Fit 是否有进一步的改进空间？（思考题回答）

有，主要体现在外部碎片与查找开销：

1. **外部碎片**：每次从低地址端切割，反复分配与释放后，低地址区域堆积大量「切剩的小碎块」，导致后续大块分配需要扫描更远，甚至总量足够却找不到连续块。
2. **查找是 O(n) 线性扫描**：空闲块越多，分配越慢。可用按地址或大小索引的平衡树把查找降到 O(log n)，或采用分离空闲链表（segregated free lists）按大小分级。
3. **头部磨损与局部性差**：每次都从链表头开始找，头部区域被反复切割，碎片集中且缓存局部性不佳。

可选的改进方向包括 Best-Fit（选最贴合、剩余最小的块）、Next-Fit（从上次位置继续搜索）、Buddy System（按 2 的幂拆分与快速合并，从根本上控制外部碎片）以及 SLUB（对小对象单独管理，避免页级浪费）。本实验后续任务二实现 Best-Fit，正是对 First-Fit 选择策略的一次改进。

---

### 任务二：实现 Best-Fit 连续物理内存分配算法

**负责人：** 2413396-范珂祎

#### 模块功能描述

**需要实现/修改的函数：**

```c
static void best_fit_init(void);
static void best_fit_init_memmap(struct Page *base, size_t n);
static struct Page *best_fit_alloc_pages(size_t n);
static void best_fit_free_pages(struct Page *base, size_t n);
static size_t best_fit_nr_free_pages(void);
// 并在 pmm.c 的 init_pmm_manager() 中把 pmm_manager 切换为 best_fit_pmm_manager
```

**功能说明：**

实现位于 [code/kern/mm/best_fit_pmm.c](../code/kern/mm/best_fit_pmm.c)，保留 `lab2` 注释并将本模块的 `YOUR CODE` 替换为学号 `2413396`；分配器切换位于 [code/kern/mm/pmm.c](../code/kern/mm/pmm.c)。

Best-Fit 与任务一理解的 First-Fit 使用相同的数据结构：一条按物理地址升序的空闲块双向链表 `free_list`，以及空闲页总数 `nr_free`。两者的区别只在 `alloc_pages` 的选择策略——First-Fit 取第一个足够大的块，Best-Fit 遍历整条链表，取「满足需求且剩余最小」的块，从而减少切割后产生的碎块，提高大块内存的可利用率。

因此本模块只在 First-Fit 的基础上改写分配选择逻辑，初始化、登记空闲区、回收合并、空闲页计数均沿用 `default_pmm.c` 的结构。`page_init()` 建立空闲区后，`check_alloc_page()` 调用当前分配器的 `best_fit_check()`；其中断言 `p0 + 4 == p1` 能区分两种算法：只有真正选择「最贴合」的块时才能通过，从而验证实现的是 Best-Fit 而非 First-Fit。

#### 核心实现说明

1. `best_fit_init`：`list_init(&free_list)` 置空链表，`nr_free = 0`。
2. `best_fit_init_memmap`：与 First-Fit 一致，逐页断言保留状态后清空 `flags/property/ref`，头页 `property = n` 并置 `PG_property`，按地址升序插入，`nr_free += n`。
3. `best_fit_alloc_pages`：先判断 `n > nr_free` 返回 NULL；否则遍历 `free_list`，记录 `property >= n` 且 `property` 最小的块；切块与 First-Fit 相同——整块切走，或从低地址端切出剩余块并保持地址升序，`nr_free -= n`，返回头页并清除 `PG_property`。
4. `best_fit_free_pages`：与 First-Fit 一致，按地址升序插入后，先向后（低地址）再向前（高地址）合并相邻空闲块。
5. `best_fit_nr_free_pages`：返回 `nr_free`。
6. 在 `pmm.c` 的 `init_pmm_manager()` 中令 `pmm_manager = &best_fit_pmm_manager`，使 `page_init()`、`check_alloc_page()` 实际走 Best-Fit。

#### 最终提示词

以下是经过迭代优化后，最终成功实现该功能的提示词：

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

#### 实现迭代过程

实际过程为一次实现、多轮验证。Best-Fit 算法代码一次写对、没有修复轮次；多轮都发生在启动骨架与验证方式上，与 `alloc_pages` 的选择策略无关。

1. 实现。在 `best_fit_pmm.c` 中实现 `best_fit_init`、`best_fit_init_memmap`、`best_fit_alloc_pages`、`best_fit_free_pages`、`best_fit_nr_free_pages` 五个函数并补齐 `best_fit_pmm_manager` 结构体；在 `pmm.c` 的 `init_pmm_manager()` 中把 `pmm_manager` 切换为 `best_fit_pmm_manager`。
2. 首次运行：内核停在 OpenSBI。`make qemu` 能完成交叉编译、链接和 objcopy，但内核停在 OpenSBI 横幅、没有任何内核输出。对照指导书发现教程按 QEMU 4.1.1 + OpenSBI v0.6（fw_jump）设计，本机是 QEMU 8.2.2 + OpenSBI v1.3（fw_dynamic）：原 Makefile 用 `-device loader,file=...,addr=0x80200000`，在 fw_dynamic 下 OpenSBI 的跳转地址为 0，直接跳到地址 0 卡死。
3. 第二轮：改 `-kernel` 后仍卡死。把 qemu/debug/test 目标的加载方式改为 `-kernel $(UCOREIMG)` 后，OpenSBI 能跳转，但切到 Sv39 后仍在物理地址 0x80200000 上取指，触发 `exec_page_fault`（cause 0x0c）。定位到 `entry.S` 的 `boot_page_table_sv39` 只映射了高半区（entry 511），缺少低地址恒等映射。
4. 第三轮：补恒等映射后正常启动。在 `boot_page_table_sv39` 增加 entry 2 的 0x80000000→0x80000000 恒等映射，并把 `boot_hartid`/`boot_dtb` 的保存挪到 `sfence.vma` 之后。内核正常进入 `kern_init`，输出 `memory management: best_fit_pmm_manager` 与 `check_alloc_page() succeeded!`，其中 `best_fit_check()` 里区分 Best-Fit 与 First-Fit 的断言 `p0 + 4 == p1` 通过。
5. 评分：`make clean` 后才出逐行打分。首次 `make test` 复用了 `make qemu` 的旧 `.o`（没有 `-D ucore_test`），没有逐行打分；`make clean && make test` 后出现 `grading: 1 / 6 points` 至 `6 / 6 points`。

上述启动与评分的排障属于骨架环境与验证方式，不是 Best-Fit 算法错误；没有通过修改断言、补写其他分配器或伪造输出来完成验证。实现、启动排障与评分的实际提示词见 [prompt.md](./prompt.md)。

---


### 任务三：buddy system（伙伴系统）分配算法

**负责人：** 2413406-黄婉文

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

以下是编写测试函数的提示词：

````markdown

伙伴系统测试：

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

本测试从初始状态一致性开始，遍历所有 order 空闲链表，检查每个节点首页 PageProperty 为真、property 等于所在 order、首页页号按 order 对齐，且所有链表累加的块大小与 nr_free_pages() 一致，以此作为后续测试的基础；随后只通过 alloc_pages、free_pages、nr_free_pages 三个公共接口，依次验证基本分配、非 2 的幂分配、分裂、伙伴合并、递归合并、空闲不足和多次分配释放后的计数一致性等核心行为。

这些测试覆盖的边界情况包括：请求页数不是 2 的幂、目标 order 没有空闲块必须分裂、伙伴不空闲不能合并、伙伴空闲需要合并、合并后还能继续向上合并需要递归、空闲不足必须失败且不破坏状态、块首页必须按 order 对齐等。测试只依赖公共接口，并且每条测试都有明确断言，细致检验管理器功能以及反馈错误，因此具备有效性。

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


### Challenge：任意大小的内存单元 slub 分配算法

**负责人：** 2413406-黄婉文

[按照Challenge的具体要求进行解答]

---


### 分支任务：gdb 调试页表查询过程

**负责人：** 2413406-黄婉文



---

## 五、测试与验证

### 5.1 成员 A：首次构建与运行

首次使用正式实现，未接入专项驱动。通过命令行临时排除未完成的 Best-Fit 编译单元，没有修改 Makefile 或测试断言：

```bash
source ~/compiler-lab/env.sh
cd ~/os-lab/lab2-member-a/code
filter='listf_cc=$(filter-out kern/mm/best_fit_pmm.c,$(call listf,$(1),$(CTYPE)))'
make -B "$filter"
timeout --signal=TERM --kill-after=2s 10s make qemu \
  QEMU="$HOME/os-lab/tools/qemu-4.1.1/bin/qemu-system-riscv64" "$filter"
```

交叉编译、链接及 `objcopy` 均成功，实际日志中的关键结果如下：

```text
memory management: default_pmm_manager
Physical memory detected: [0x0000000080000000, 0x0000000088000000), size 0x0000000008000000
Manageable physical memory end: 0x0000000088000000, descriptors: 32768
Page descriptors: VA 0xffffffffc0207000, PA [0x0000000080207000, 0x0000000080347000)
Initial free pages: 31929, aligned start PA 0x0000000080347000
check_alloc_page() succeeded!
```

首次运行有 **31929 个初始空闲页**。结果依据是实际布局输出及原有 First-Fit 检查通过；退出码 124 只是内核进入循环后由超时终止 QEMU，不能单独作为成功依据。

![首次构建与运行日志回看](./images/member-a-lab2-first-run.png)

*首次构建与运行日志回看*

原始材料：[构建日志](./verification/member-a/first-run/build.log)、[运行日志](./verification/member-a/first-run/run.log)、[执行命令与输出](./verification/member-a/first-run/session.log)。

### 5.2 原始专项测试与边界场景

在独立副本中增加测试接入点，实际 `page_init()` 函数体保持不变。默认场景在 `page_init()` 返回后、First-Fit 检查之前调用原 `test_page_init()`，测试条件未修改，输出为：

```text
[Test 1] Page Init: PASS
check_alloc_page() succeeded!
```

![模块一专项测试日志回看](./images/member-a-lab2-page-init-test.png)

*模块一专项测试日志回看*

两张图均为已保存的真实运行日志回看，分别对应首次正式镜像和专项测试副本，没有重新运行实验。

本轮 13 个动态场景均由 QEMU 4.1.1 执行。前三项读取真实设备树信息，其余在接口处注入返回值，没有另写初始化算法代替实际函数。

| 场景 | 验证方式 | 预期结果 | 本轮实际结果 |
|------|----------|----------|--------------|
| 默认 128 MiB | 真实 DTB；原始专项测试 | 原测试和 First-Fit 通过 | 通过；32768 个描述符、31928 个空闲页 |
| 64 MiB | `-m 64M`；独立布局检查 | 使用实际范围，不超过 RAM 末端 | 通过；末端 `0x84000000`，16384 个描述符、15704 个空闲页；First-Fit 通过 |
| 256 MiB | `-m 256M`；独立布局检查 | 不超过工程管理上界 | 通过；探测到 256 MiB，管理末端 `0x88000000`，32768 个描述符、31928 个空闲页；First-Fit 通过 |
| 起点为零 | 接口返回值注入 | 明确 panic，不注册空闲页 | 符合预期；候选区与哨兵检查通过 |
| 大小为零 | 接口返回值注入 | 同上 | 符合预期 |
| 起点加大小溢出 | 接口返回值注入 | 在溢出加法前拒绝 | 符合预期 |
| 内存基址不兼容 | 接口返回值注入 | 拒绝当前映射不支持的基址 | 符合预期 |
| 内存无法覆盖内核起点 | 接口返回值注入 | 终止初始化，不写描述符 | 符合预期 |
| 内存无法覆盖内核尾部 | 接口返回值注入 | 同上 | 符合预期 |
| 描述符数组放不下 | 接口返回值注入 | 写入数组前拒绝 | 符合预期；`pages` 仍为 NULL |
| 数组放得下，但无完整空闲页 | 注入带非对齐末端的范围 | 正常返回，不调用 `init_memmap()` | 通过；空闲页为 0，调用次数为 0 |
| 无空闲页，末端恰好页对齐 | 接口返回值注入 | 同上 | 通过；调用次数为 0 |
| 64 MiB 加 123 字节 | 接口返回值注入 | 不把尾部不足一页的空间加入分配器 | 通过；管理末端仍为 `0x84000000`，First-Fit 通过 |

异常场景先检查预期错误信息、注册次数和哨兵，再进入框架原有的 `__panic()`。其中的 panic 是预期结果；这些 QEMU 进程最终由超时结束。无空闲页场景只在测试副本中跳过后续要求分配成功的 First-Fit 检查，没有改动原检查函数。

专项测试加入驱动后，内核末端从首次运行的 `0xFFFFFFFFC0206078` 变为 `0xFFFFFFFFC02070C8`，对齐后的描述符起点向后移动一页。因此专项默认运行的空闲页是 **31928**，与首次正式运行的 **31929** 分别记录。

证据：[默认专项日志](./verification/member-a/boundaries/logs/default-128MiB.log)、[256 MiB 日志](./verification/member-a/boundaries/logs/native-256MiB.log)、[无完整空闲页日志](./verification/member-a/boundaries/logs/no-free-partial-tail.log)、[13 场景结果与日志索引](./verification/member-a/boundaries/logs/results.md)。

### 5.3 验证范围与材料

静态检查核对了范围加法、描述符容量检查、循环上界、页对齐和 `pa2page()` 的调用条件。动态检查核对了实际输出、逐页状态和地址转换、注册区间、调用计数及监控区域的哨兵。哨兵未变化只说明监控区域未被破坏，不是对任意地址和所有输入的形式化证明。

正式函数体与测试副本函数体哈希一致，原始测试文件与接入副本也一致；执行前后正式源码哈希未变化，记录见 [manifest.json](./verification/member-a/boundaries/logs/manifest.json)。本轮没有修改正式实现，不存在代码修复后的复验轮次。

未验证损坏 DTB 原始数据的解析、其他硬件映射、Best-Fit、Buddy 或完整 Lab2 评分。未执行包含批量删除命令的 `clean`、`grade` 流程，也不将本轮结果写成完整评分通过。

必要日志、驱动和使用方法见 [成员 A 验证材料说明](./verification/member-a/README.md)。首次运行和原始专项测试的两张真实截图已补齐；[截图状态与回看命令](./verification/member-a/screenshots.md) 保留对应路径和可选展示项。其他成员的测试结论与截图由负责成员补充。

### 5.4 成员 B：Best-Fit 分配器验证

在 WSL Ubuntu 24.04 + riscv64-unknown-elf-gcc 15.1.0 + QEMU 8.2.2（OpenSBI v1.3）环境下，用 `make clean && make test` 编译并运行带 `-D ucore_test` 的镜像，逐行评分为：

```text
memory management: best_fit_pmm_manager
grading: 1 / 6 points
grading: 2 / 6 points
grading: 3 / 6 points
grading: 4 / 6 points
grading: 5 / 6 points
grading: 6 / 6 points
check_alloc_page() succeeded!
```

`grading: 6 / 6 points` 说明 Best-Fit 分配器通过全部评分点；`memory management: best_fit_pmm_manager` 确认 `init_pmm_manager()` 已切换到 Best-Fit；`check_alloc_page() succeeded!` 说明 `best_fit_check()` 通过，其中区分 Best-Fit 与 First-Fit 的关键断言 `p0 + 4 == p1` 成立，证明分配器确实选择了「满足需求且剩余最小」的空闲块，而非 First-Fit 的「第一个足够大的块」。

![Best-Fit 评分日志回看](./images/member-b-lab2-best-fit-test.png)

*Best-Fit 评分日志回看*

首次 `make test` 因复用了 `make qemu` 的旧 `.o`（未带 `-D ucore_test`）而没有逐行打分；`make clean` 后重新 `make test` 才得到上述评分。这表明评分开关依赖 `CFLAGS2`（`-D ucore_test`），验证前必须先 `make clean`，避免旧目标文件被复用。


### 模块二任务三

伙伴系统实现后运行make qemu验证：
![Buddy System implement](./images/buddy_system_impl.png)

---

## 六、实验总结与收获

本章汇总各成员对操作系统的理解、AI 协作开发的经验以及待补事项。

## 对操作系统的理解

模块一：

| 模块一内容 | 与 OS 原理的对应关系 |
|------------|---------------------|
| 读取设备树内存范围 | OS 需要获得硬件资源信息；本实验通过已有设备树解析接口获取单段 RAM，并非通用硬件探测实现 |
| 为物理页建立 `Page` 描述符 | 用软件数据结构记录物理资源的状态；描述符是管理数据，不是物理页本身，也不是页表项 |
| 保留内核和描述符数组 | 已占用内存不能重新分配，管理数据本身同样需要占用资源 |
| 虚实地址转换 | 内核通过已有映射访问物理内存；本模块使用偏移关系，没有实现新的页表映射 |
| 页面对齐和空闲区注册 | 页级分配只接收完整页面，必须排除首尾对齐空间和超出管理上界的部分 |
| 通过 `pmm_manager` 交接 | 初始化提供可用资源，分配器决定如何分配和回收，体现接口与具体策略的分工 |

本模块尚未实现进程地址空间、按需分页、缺页处理、页面置换、交换、并发分配保护和完整的多段内存管理。启动页表和分配算法虽然是依赖条件，但不能算作成员 A 新实现的内容。

模块二任务一、任务二的理解：

| 模块二内容 | 与 OS 原理的对应关系 |
|------------|---------------------|
| `free_list` + `nr_free` | 分配器用空闲块双向链表组织可分配页，`nr_free` 记录空闲总量，是连续分配器最核心的状态 |
| 分配策略（First-Fit 取第一个足够大的块，Best-Fit 取剩余最小的块） | 同一「分配/回收」接口下可以有不同策略；策略只改 `alloc_pages` 的选择，体现接口与实现的分离 |
| 切块时剩余块插回链表并保持地址升序 | 分配器必须维护链表不变量，否则后续分配、合并会出错 |
| 回收时与相邻空闲块合并 | 避免空闲内存碎片化，是连续分配算法能否长期可用的关键 |
| `p0 + 4 == p1` 断言 | 用可观察行为区分算法：Best-Fit 选最贴合的块导致特定地址关系，与 First-Fit 不同 |

Best-Fit 相比 First-Fit 减少了切割后的碎块，但要遍历整条链表、分配代价更高；两者都没有从根本上解决外部碎片，这正是后续 Buddy、SLUB 的出发点。

## AI 协作开发的经验

成员 A 先要求检查工程，再按实际接口提出需求，随后分别进行运行验证和边界验证。提示词中把修改范围、异常处理和成功状态写清楚，便于检查生成代码是否符合预期。例如，区分 `npage` 与页面总数、要求先检查整个描述符数组能否放下、要求无空闲页时不调用 `init_memmap()`，都能对应到具体检查。

不能只依据 AI 的说明或 QEMU 的退出码判断结果。实际核对源码、日志、函数体哈希和测试驱动后，才将本轮场景记录为通过。实现一次通过不需要编造错误修复；后续测试对规格的补充也应与正式代码修改次数分开记录。其他成员的协作经验由各自补充。

成员 B 的经验：

先让 AI 阅读 `default_pmm.c`（First-Fit）理解框架，再在四段式提示词里把「区别」写清楚——[SPECIFICATION] 明确 Best-Fit 的选择规则是「property >= n 且 property 最小」，[RELY] 指明已有测试不可修改，这样生成的代码一次就选对了策略、也保住了 `p0 + 4 == p1` 断言。启动排障时，AI 通过 OpenSBI 停住的位置和 `exec_page_fault` 的 epc 定位到 QEMU 版本差异与 `entry.S` 缺恒等映射，说明把「现象 + 出错地址 + 版本信息」一起给 AI，比只贴日志更容易定位。评分时 AI 指出 `make test` 必须先 `make clean`，因为评分开关在 `CFLAGS2` 里，旧 `.o` 会掩盖 `-D ucore_test`——这提醒我验证前要区分「代码问题」和「构建/环境问题」。

## 待补事项

成员 A：模块一代码、报告、实际提示词、验证材料和两张必要截图已整理，工具与模型信息已确认，个人模块具备提交条件。后续仍需履行统筹职责，整合其他成员材料；256 MiB 和异常场景截图为可选项。

全组：由其他成员补充自己的算法说明、实际提示词、问题与验证记录，核对个人工具信息；完成集成验证并统一报告与分工；全部完成后填写日期，按小组要求整理到 `lab2` 分支。本稿不提前认定这些项目已完成。

成员 B：模块二任务一、任务二代码、报告、实际提示词和 Best-Fit 评分截图已整理，工具与模型信息已确认，个人模块具备提交条件。启动骨架修复（Makefile `-kernel`、`entry.S` 恒等映射）需与全组说明，避免其他成员在各自环境遇到同样问题。

---

