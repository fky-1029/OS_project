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

### Prompt 2：实现测试函数


## 功能模块二challenge

**2413406-黄婉文**

### Prompt 1：slub算法代码实现

### Prompt 2：实现测试函数