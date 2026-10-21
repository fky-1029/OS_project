#include <buddy_system_pmm.h>
#include <list.h>
#include <pmm.h>

//lab2 function2-3 2413406

/*
 * Buddy System allocator.
 *
 * 每个空闲块由 2^order 个连续物理页组成，且块首页的物理页号按 2^order 对齐，
 * 因此把块首页页号的第 order 位取反就得到它的伙伴块。空闲块用首页描述符表示：
 * PG_property 置 1，property 记录块的 order；已分配的块保留 property 中的
 * order 但清除 PG_property，这样释放时才知道块的实际大小。
 */

#define MAX_ORDER 10  // 最大块为 2^10 页 = 4 MiB

static list_entry_t free_lists[MAX_ORDER + 1];
static unsigned int nr_free;

// 块的 order 对应的页数
#define BUDDY_PAGES(order) ((size_t)1 << (order))

static void
buddy_init(void) {
    for (unsigned int order = 0; order <= MAX_ORDER; order++) {
        list_init(&free_lists[order]);
    }
    nr_free = 0;
}

// 不小于 n 的最小 2 的幂对应的 order，调用者需保证 n <= 2^MAX_ORDER
static unsigned int
buddy_order_of(size_t n) {
    unsigned int order = 0;
    while (order < MAX_ORDER && BUDDY_PAGES(order) < n) {
        order++;
    }
    return order;
}

static inline struct Page *
buddy_page_of_ppn(uintptr_t ppn) {
    return &pages[ppn - nbase];
}

// 把一个满足对齐要求的 2^order 页块登记为空闲块
static void
buddy_register_block(struct Page *block, unsigned int order) {
    block->property = order;
    SetPageProperty(block);
    list_add(&free_lists[order], &block->page_link);
    nr_free += (unsigned int)BUDDY_PAGES(order);
}

/*
 * 把 [base, base + n) 拆成若干按 order 对齐的 2 的幂大小的块并登记。
 * 每次取不超过剩余页数的最大 order，再向下调整到首页页号满足对齐要求，
 * 这样既不会越过区间末尾，也不会产生跨区间的块。
 */
static void
buddy_register_range(struct Page *base, size_t n) {
    struct Page *p = base;
    size_t remaining = n;

    while (remaining > 0) {
        unsigned int order = 0;
        while (order < MAX_ORDER && BUDDY_PAGES(order + 1) <= remaining) {
            order++;
        }
        while (order > 0 &&
               (page2ppn(p) & ((uintptr_t)BUDDY_PAGES(order) - 1)) != 0) {
            order--;
        }
        buddy_register_block(p, order);
        p += BUDDY_PAGES(order);
        remaining -= BUDDY_PAGES(order);
    }
}

static void
buddy_init_memmap(struct Page *base, size_t n) {
    if (n == 0) {
        return;
    }

    // 这段页即将交给分配器，清除保留标记并复位描述符。
    for (struct Page *p = base; p != base + n; p++) {
        p->flags = 0;
        p->property = 0;
        set_page_ref(p, 0);
    }

    buddy_register_range(base, n);
}

static struct Page *
buddy_alloc_pages(size_t n) {
    if (n == 0 || n > BUDDY_PAGES(MAX_ORDER) || n > nr_free) {
        return NULL;
    }

    unsigned int order = buddy_order_of(n);
    unsigned int cur = order;
    while (cur <= MAX_ORDER && list_empty(&free_lists[cur])) {
        cur++;
    }
    if (cur > MAX_ORDER) {
        return NULL;
    }

    // 从较大的空闲块上摘下，并逐级分裂出伙伴块。
    struct Page *page = le2page(list_next(&free_lists[cur]), page_link);
    list_del(&page->page_link);
    while (cur > order) {
        cur--;
        struct Page *buddy = page +
            (uintptr_t)BUDDY_PAGES(cur);
        buddy->property = cur;
        SetPageProperty(buddy);
        list_add(&free_lists[cur], &buddy->page_link);
    }

    ClearPageProperty(page);
    page->property = order;
    nr_free -= (unsigned int)BUDDY_PAGES(order);
    return page;
}

static void
buddy_free_pages(struct Page *base, size_t n) {
    assert(n > 0);
    assert(base != NULL);
    // 只有通过 buddy_alloc_pages 取得的块首页才能释放，其 order 记录在 property 中。
    assert(!PageProperty(base));

    unsigned int order = base->property;
    assert(order <= MAX_ORDER);

    nr_free += (unsigned int)BUDDY_PAGES(order);
    base->property = order;
    SetPageProperty(base);
    list_add(&free_lists[order], &base->page_link);

    // 伙伴块空闲且同 order 时合并，合并后继续向上一级检查。
    while (order < MAX_ORDER) {
        uintptr_t ppn = page2ppn(base);
        uintptr_t buddy_ppn = ppn ^ (uintptr_t)BUDDY_PAGES(order);
        if (buddy_ppn < nbase || buddy_ppn >= npage ||
            buddy_ppn + (uintptr_t)BUDDY_PAGES(order) > npage) {
            break;
        }

        struct Page *buddy = buddy_page_of_ppn(buddy_ppn);
        if (!PageProperty(buddy) || buddy->property != order) {
            break;
        }

        list_del(&buddy->page_link);
        list_del(&base->page_link);
        ClearPageProperty(buddy);
        buddy->property = 0;
        if (buddy < base) {
            base = buddy;
        }

        order++;
        base->property = order;
        SetPageProperty(base);
        list_add(&free_lists[order], &base->page_link);
    }
}

static size_t
buddy_nr_free_pages(void) {
    return nr_free;
}

static void buddy_system_check(void);

const struct pmm_manager buddy_system_pmm_manager = {
    .name = "buddy_system_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_system_check,
};

/* ----------------------------- 测试部分 ----------------------------- */

// 统计某个 order 空闲链表上的块数
static size_t
buddy_count_list(unsigned int order) {
    size_t count = 0;
    for (list_entry_t *le = list_next(&free_lists[order]);
         le != &free_lists[order]; le = list_next(le)) {
        count++;
    }
    return count;
}

// 清空所有 order 的链表和计数，并清除全部分配块标记
static void
buddy_clear_all_marks(void) {
    size_t total = npage - nbase;
    for (size_t i = 0; i < total; i++) {
        pages[i].property = 0;
        ClearPageProperty(&pages[i]);
    }
}

static void
buddy_reset_lists(void) {
    for (unsigned int order = 0; order <= MAX_ORDER; order++) {
        list_init(&free_lists[order]);
    }
    nr_free = 0;
    buddy_clear_all_marks();
}

/*
 * 检查空闲链表的全部不变量：每个节点都是空闲块首页、property 与所在 order
 * 一致、首页页号按 order 对齐、块不越过描述符范围，且所有块页数之和等于
 * 管理器的空闲页计数。
 */
static void
buddy_check_lists(void) {
    size_t total = 0;

    for (unsigned int order = 0; order <= MAX_ORDER; order++) {
        list_entry_t *le = &free_lists[order];
        while ((le = list_next(le)) != &free_lists[order]) {
            struct Page *p = le2page(le, page_link);
            uintptr_t ppn = page2ppn(p);

            assert(PageProperty(p));
            assert(p->property == order);
            assert(ppn >= nbase && ppn < npage);
            assert((ppn & ((uintptr_t)BUDDY_PAGES(order) - 1)) == 0);
            assert(ppn + (uintptr_t)BUDDY_PAGES(order) <= npage);

            total += BUDDY_PAGES(order);
        }
    }

    assert(total == (size_t)nr_free);
    assert(nr_free_pages() == (size_t)nr_free);
}

/*
 * 从页描述符重建初始空闲区。check 期间会破坏性地重置内部链表，结束后需要
 * 恢复。初始空闲区就是所有清除了 PG_reserved 的连续页，用与 init_memmap
 * 完全相同的拆分规则重新登记，即可得到启动时的空闲链表。
 */
static void
buddy_rebuild_from_descriptors(void) {
    size_t total = npage - nbase;

    buddy_reset_lists();

    size_t i = 0;
    while (i < total) {
        if (PageReserved(&pages[i])) {
            i++;
            continue;
        }
        size_t start = i;
        while (i < total && !PageReserved(&pages[i])) {
            i++;
        }
        buddy_register_range(&pages[start], i - start);
    }
}

static void
buddy_system_check(void) {
    // 1. 初始状态一致性：链表、order、对齐、计数
    buddy_check_lists();

    size_t saved_free = nr_free;
    if (saved_free == 0) {
        return;
    }

    // 2. 取一个完整的空闲块作为测试场地，保证地址对齐已知
    unsigned int arena_order = MAX_ORDER;
    while (arena_order > 0 && list_empty(&free_lists[arena_order])) {
        arena_order--;
    }
    if (list_empty(&free_lists[arena_order])) {
        return;
    }
    struct Page *arena = le2page(list_next(&free_lists[arena_order]),
                                 page_link);
    size_t arena_pages = BUDDY_PAGES(arena_order);

    // 后续用例需要至少 8 页的场地，空闲区太小时只做一致性检查。
    if (arena_order < 3) {
        return;
    }

    // 3. 把管理器收缩到只剩这个块，避免测试结果依赖外部空闲布局
    buddy_reset_lists();
    buddy_register_range(arena, arena_pages);

    assert(nr_free == (unsigned int)arena_pages);
    assert(buddy_count_list(arena_order) == 1);

    // 4. 基本分配：1 页请求得到 order 0 的块，且逐级分裂出伙伴块
    struct Page *p0 = alloc_pages(1);
    assert(p0 == arena);
    assert(!PageProperty(p0));
    assert(p0->property == 0);
    assert(page_ref(p0) == 0);
    assert(nr_free == (unsigned int)arena_pages - 1);
    for (unsigned int order = 0; order < arena_order; order++) {
        assert(buddy_count_list(order) == 1);
    }
    assert(list_empty(&free_lists[arena_order]));

    // 5. 伙伴仍被占用时不能合并
    struct Page *p1 = alloc_pages(1);
    assert(p1 == arena + 1);
    assert(nr_free == (unsigned int)arena_pages - 2);

    free_pages(p1, 1);
    assert(nr_free == (unsigned int)arena_pages - 1);
    assert(buddy_count_list(0) == 1);
    assert(buddy_count_list(1) == 0);

    // 6. 释放另一页，两个伙伴应当递归合并回最初的整块
    free_pages(p0, 1);
    assert(nr_free == (unsigned int)arena_pages);
    assert(buddy_count_list(arena_order) == 1);
    for (unsigned int order = 0; order < arena_order; order++) {
        assert(buddy_count_list(order) == 0);
    }

    // 7. 非 2 的幂请求：3 页应得到 4 页的块，释放后完全归还
    p0 = alloc_pages(3);
    assert(p0 == arena);
    assert(p0->property == 2);
    assert(nr_free == (unsigned int)arena_pages - 4);
    free_pages(p0, 3);
    assert(nr_free == (unsigned int)arena_pages);
    assert(buddy_count_list(arena_order) == 1);

    // 8. 空闲页不足：失败且不改变链表和计数
    buddy_reset_lists();
    buddy_register_range(arena, arena_pages / 2);
    assert(nr_free == (unsigned int)(arena_pages / 2));
    p0 = alloc_pages((size_t)nr_free + 1);
    assert(p0 == NULL);
    assert(nr_free == (unsigned int)(arena_pages / 2));
    assert(buddy_count_list(arena_order - 1) == 1);

    // 9. 非对齐起点 + 非 2 的幂长度：[arena+1, arena+6) 应拆成 1+2+2
    buddy_reset_lists();
    buddy_register_range(arena + 1, 5);
    assert(nr_free == 5);
    assert(buddy_count_list(0) == 1);
    assert(buddy_count_list(1) == 2);

    // 总空闲页够但没有 order 2 的连续块，4 页请求必须失败
    assert(alloc_pages(4) == NULL);
    assert(nr_free == 5);

    p0 = alloc_pages(2);
    assert(p0 != NULL);
    free_pages(p0, 2);
    assert(nr_free == 5);

    // 10. 恢复启动时的空闲布局并复核
    buddy_rebuild_from_descriptors();
    assert(nr_free == (unsigned int)saved_free);
    buddy_check_lists();
}
