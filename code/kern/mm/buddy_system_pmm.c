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

static void buddy_check(void);

const struct pmm_manager buddy_system_pmm_manager = {
    .name = "buddy_system_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};

/* ----------------------------- 测试部分 ----------------------------- */

// 测试期间临时借出的块，用于构造"空闲不足"和"只留大块"的场景
#define BUDDY_TEST_MAX_BLOCKS 96

static struct Page *test_held_page[BUDDY_TEST_MAX_BLOCKS];
static size_t test_held_pages[BUDDY_TEST_MAX_BLOCKS];

// 测试前后的空闲布局快照
struct buddy_snapshot {
    size_t order_count[MAX_ORDER + 1];
    size_t free_pages;
};

static size_t
buddy_count(unsigned int order) {
    size_t count = 0;
    for (list_entry_t *le = list_next(&free_lists[order]);
         le != &free_lists[order]; le = list_next(le)) {
        count++;
    }
    return count;
}

static void
buddy_take_snapshot(struct buddy_snapshot *s) {
    for (unsigned int order = 0; order <= MAX_ORDER; order++) {
        s->order_count[order] = buddy_count(order);
    }
    s->free_pages = nr_free_pages();
}

static void
buddy_expect_snapshot(const struct buddy_snapshot *s) {
    assert(nr_free_pages() == s->free_pages);
    for (unsigned int order = 0; order <= MAX_ORDER; order++) {
        assert(buddy_count(order) == s->order_count[order]);
    }
}

// Case 1：初始状态一致性
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

    assert(total == nr_free_pages());
}

/*
 * 找一个"本 order 没有空闲块、但更大 order 有空闲块"的 order，
 * 用于验证分配时的分裂行为。找不到返回 -1。
 */
static int
buddy_find_split_order(void) {
    for (unsigned int order = 0; order < MAX_ORDER; order++) {
        if (!list_empty(&free_lists[order])) {
            continue;
        }
        for (unsigned int bigger = order + 1; bigger <= MAX_ORDER; bigger++) {
            if (!list_empty(&free_lists[bigger])) {
                return (int)order;
            }
        }
    }
    return -1;
}

static void
buddy_check(void) {
    struct buddy_snapshot before;

    // Case 1：遍历所有 order 链表，检查首页标记、order、对齐和计数
    buddy_check_lists();
    buddy_take_snapshot(&before);

    if (nr_free_pages() == 0) {
        return;
    }

    // Case 2：2 的幂请求，分配 1 页
    {
        size_t free_before = nr_free_pages();
        struct Page *p = alloc_pages(1);

        assert(p != NULL);
        assert(!PageProperty(p));
        assert(p->property == 0);
        assert(page_ref(p) == 0);
        assert(nr_free_pages() == free_before - 1);

        free_page(p);
        assert(nr_free_pages() == free_before);
        buddy_expect_snapshot(&before);
    }

    // Case 3：非 2 的幂请求，分配 3 页应得到 order 2 的块
    if (nr_free_pages() >= 4) {
        size_t free_before = nr_free_pages();
        struct Page *p = alloc_pages(3);

        assert(p != NULL);
        assert(!PageProperty(p));
        assert(p->property == 2);
        assert(nr_free_pages() == free_before - 4);

        free_pages(p, 3);
        assert(nr_free_pages() == free_before);
        buddy_expect_snapshot(&before);
    }

    // Case 4：目标 order 没有空闲块，从更大的块分裂
    int split_order = buddy_find_split_order();
    if (split_order >= 0) {
        unsigned int order = (unsigned int)split_order;
        size_t n = BUDDY_PAGES(order);
        size_t free_before = nr_free_pages();
        struct Page *p = alloc_pages(n);
        struct Page *buddy;

        assert(p != NULL);
        assert(!PageProperty(p));
        assert(p->property == order);
        assert(nr_free_pages() == free_before - n);

        // 分裂后目标 order 上应当恰好出现一个空闲伙伴块，就是 p 的另一半
        assert(buddy_count(order) == 1);
        buddy = le2page(list_next(&free_lists[order]), page_link);
        assert(buddy == p + n);
        assert((page2ppn(buddy) & (n - 1)) == 0);

        free_pages(p, n);
        assert(nr_free_pages() == free_before);
        assert(buddy_count(order) == 0);
        buddy_expect_snapshot(&before);
    }

    // Case 5：分配两个互为伙伴的块，先释放一个不合并，再释放另一个合并
    if (split_order >= 0) {
        unsigned int order = (unsigned int)split_order;
        size_t n = BUDDY_PAGES(order);
        size_t free_before = nr_free_pages();
        struct Page *p = alloc_pages(n);
        struct Page *q;

        assert(p != NULL);
        q = alloc_pages(n);
        assert(q != NULL);
        assert(q == p + n);
        assert(nr_free_pages() == free_before - 2 * n);

        // 伙伴 p 仍被占用，q 释放后不能合并
        free_pages(q, n);
        assert(nr_free_pages() == free_before - n);
        assert(buddy_count(order) == 1);
        assert(le2page(list_next(&free_lists[order]), page_link) == q);

        // 释放 p 后两个伙伴合并成更大的块，直到回到原来的大块
        free_pages(p, n);
        assert(nr_free_pages() == free_before);
        assert(buddy_count(order) == 0);
        buddy_expect_snapshot(&before);
    }

    // Case 6：递归合并
    {
        size_t held = 0;
        int drained = 1;

        // 先把所有小 order 的块借出来，只留下 order MAX_ORDER 的大块
        for (unsigned int order = 0; order < MAX_ORDER; order++) {
            while (!list_empty(&free_lists[order])) {
                struct Page *h;
                if (held == BUDDY_TEST_MAX_BLOCKS) {
                    drained = 0;
                    break;
                }
                h = alloc_pages(BUDDY_PAGES(order));
                assert(h != NULL);
                test_held_page[held] = h;
                test_held_pages[held] = BUDDY_PAGES(order);
                held++;
            }
            if (!drained) {
                break;
            }
        }

        if (drained && !list_empty(&free_lists[MAX_ORDER])) {
            size_t free_before = nr_free_pages();
            struct Page *chain[MAX_ORDER];

            // 从一个 order MAX_ORDER 的块开始，按 order 从小到大分配出一条链
            chain[0] = alloc_pages(1);
            assert(chain[0] != NULL);
            assert(chain[0]->property == 0);
            for (unsigned int order = 1; order < MAX_ORDER; order++) {
                chain[order] = alloc_pages(BUDDY_PAGES(order));
                assert(chain[order] == chain[0] + BUDDY_PAGES(order));
                assert(chain[order]->property == order);
            }

            // 按 order 升序释放：每释放一个就应当与已合并的块再合并一级
            for (unsigned int order = 0; order < MAX_ORDER; order++) {
                free_pages(chain[order], BUDDY_PAGES(order));
            }

            // 最终合并回最初的 order MAX_ORDER 块
            assert(PageProperty(chain[0]));
            assert(chain[0]->property == MAX_ORDER);
            assert(buddy_count(MAX_ORDER) == before.order_count[MAX_ORDER]);
            assert(nr_free_pages() == free_before);
        }

        for (size_t i = held; i > 0; i--) {
            free_pages(test_held_page[i - 1], test_held_pages[i - 1]);
        }
        buddy_expect_snapshot(&before);
    }

    // Case 7：空闲不足
    {
        size_t held = 0;
        int complete = 1;

        // 借走所有 order MAX_ORDER 的块，让空闲页不足以满足这种大小的请求
        while (!list_empty(&free_lists[MAX_ORDER])) {
            struct Page *h;
            if (held == BUDDY_TEST_MAX_BLOCKS) {
                complete = 0;
                break;
            }
            h = alloc_pages(BUDDY_PAGES(MAX_ORDER));
            assert(h != NULL);
            test_held_page[held] = h;
            test_held_pages[held] = BUDDY_PAGES(MAX_ORDER);
            held++;
        }

        if (complete && nr_free_pages() < BUDDY_PAGES(MAX_ORDER)) {
            size_t free_left = nr_free_pages();

            // 需要 order MAX_ORDER 的块，但已经没有这么大的空闲块
            assert(alloc_pages(BUDDY_PAGES(MAX_ORDER)) == NULL);
            assert(nr_free_pages() == free_left);

            // 请求页数超过当前空闲页数
            assert(alloc_pages(free_left + 1) == NULL);
            assert(nr_free_pages() == free_left);

            buddy_check_lists();
        }

        for (size_t i = held; i > 0; i--) {
            free_pages(test_held_page[i - 1], test_held_pages[i - 1]);
        }
        buddy_expect_snapshot(&before);
    }

    // Case 8：多次分配释放后的计数一致性
    if (nr_free_pages() >= 200) {
        size_t sizes[6] = {1, 2, 3, 8, 17, 100};
        struct Page *block[6];
        size_t free_before = nr_free_pages();

        for (int i = 0; i < 6; i++) {
            block[i] = alloc_pages(sizes[i]);
            assert(block[i] != NULL);
            assert(!PageProperty(block[i]));
            buddy_check_lists();
        }
        assert(nr_free_pages() < free_before);

        for (int i = 5; i >= 0; i--) {
            free_pages(block[i], sizes[i]);
            buddy_check_lists();
        }
        assert(nr_free_pages() == free_before);
        buddy_expect_snapshot(&before);
    }

    // 所有用例结束后，空闲布局应当与测试前一致
    buddy_check_lists();
    buddy_expect_snapshot(&before);
}
