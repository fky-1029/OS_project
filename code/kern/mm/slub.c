#include <slub.h>
#include <pmm.h>
#include <stdio.h>

//lab2 challenge 2413406

/*
 * 简化版 SLUB：对象缓存层。
 *
 * 每个 cache 负责一个 2 的幂大小的对象。cache 的 freelist 为空时向当前
 * pmm_manager 申请一个 slab（通常是一页），把 slab 切成固定大小的对象，
 * 用对象头部把空闲对象串成链表；之后分配和释放都只在这个链表上做，
 * 不再经过页级分配器。
 *
 * 每个对象前面有 8 字节头部：对象空闲时头部存 freelist 的 next 指针，
 * 对象已分配时头部存所属 cache 的指针。因此 slub_free() 只凭用户指针
 * 就能找到对象起始地址和 cache，不需要额外的页级元数据。
 */

#define SLUB_MIN_SIZE 16
#define SLUB_MAX_SIZE 4096
#define SLUB_NUM_CLASSES 9
#define SLUB_ALIGN 8

struct slub_cache;

struct slub_header {
    union {
        struct slub_header *next;   // 空闲时：freelist 中的下一个对象
        struct slub_cache *cache;   // 已分配时：对象所属的 cache
    } u;
};

struct slub_cache {
    size_t object_size;       // 用户数据区大小
    size_t total_size;        // 单个对象实际占用：头部 + 用户数据，按 8 字节对齐
    size_t objects_per_slab;  // 每个 slab 切出的对象数
    size_t slab_pages;        // 每个 slab 占用的页数
    struct slub_header *freelist;  // 空闲对象链表头
};

static struct slub_cache caches[SLUB_NUM_CLASSES];

static size_t
slub_class_size(size_t index) {
    return (size_t)SLUB_MIN_SIZE << index;
}

static void
slub_cache_init(struct slub_cache *cache, size_t object_size) {
    cache->object_size = object_size;
    cache->total_size = ROUNDUP(sizeof(struct slub_header) + object_size,
                                SLUB_ALIGN);
    cache->objects_per_slab = PGSIZE / cache->total_size;
    if (cache->objects_per_slab == 0) {
        // 4096 字节的对象加头部后超过一页，至少也要切出一个对象，
        // 不足的空间由 slab_pages 多申请一页补上。
        cache->objects_per_slab = 1;
    }
    cache->slab_pages =
        (cache->objects_per_slab * cache->total_size + PGSIZE - 1) / PGSIZE;
    if (cache->slab_pages == 0) {
        cache->slab_pages = 1;
    }
    cache->freelist = NULL;
}

/*
 * 把刚从 pmm_manager 申请到的 slab 切分成对象并串成 freelist。
 * 返回链表头，所有对象仍在 slab 内，不需要额外内存。
 */
static struct slub_header *
slub_build_slab(struct slub_cache *cache, struct Page *slab_page) {
    uintptr_t base = page2pa(slab_page) + va_pa_offset;
    struct slub_header *head = NULL;

    for (size_t i = 0; i < cache->objects_per_slab; i++) {
        struct slub_header *obj =
            (struct slub_header *)(base + i * cache->total_size);
        obj->u.next = head;
        head = obj;
    }
    return head;
}

void
slub_init(void) {
    for (size_t i = 0; i < SLUB_NUM_CLASSES; i++) {
        slub_cache_init(&caches[i], slub_class_size(i));
    }
}

void *
slub_alloc(size_t size) {
    if (size == 0 || size > SLUB_MAX_SIZE) {
        return NULL;
    }

    // 找到第一个能容纳 size 的 size class
    struct slub_cache *cache = NULL;
    for (size_t i = 0; i < SLUB_NUM_CLASSES; i++) {
        if (size <= caches[i].object_size) {
            cache = &caches[i];
            break;
        }
    }
    if (cache == NULL) {
        return NULL;
    }

    if (cache->freelist == NULL) {
        struct Page *slab_page = alloc_pages(cache->slab_pages);
        if (slab_page == NULL) {
            return NULL;
        }
        cache->freelist = slub_build_slab(cache, slab_page);
    }

    struct slub_header *obj = cache->freelist;
    cache->freelist = obj->u.next;
    obj->u.cache = cache;
    return (void *)(obj + 1);
}

void
slub_free(void *ptr) {
    if (ptr == NULL) {
        return;
    }

    struct slub_header *obj = (struct slub_header *)ptr - 1;
    struct slub_cache *cache = obj->u.cache;

    obj->u.next = cache->freelist;
    cache->freelist = obj;
}

/* ----------------------------- 测试部分 ----------------------------- */

// 16 字节档每页切出的对象数最多（170），Case 3 会多分配一个对象
#define SLUB_TEST_MAX_OBJECTS 171

static void *test_objs[SLUB_TEST_MAX_OBJECTS];

// 在用户数据区的首尾各写一个 8 字节特征值并读回校验
static void
slub_test_fill(void *obj, size_t size, uint64_t value) {
    volatile uint64_t *head = (volatile uint64_t *)obj;
    volatile uint64_t *tail =
        (volatile uint64_t *)((char *)obj + size - sizeof(uint64_t));

    *head = value;
    *tail = ~value;
    assert(*head == value);
    assert(*tail == ~value);
}

static void
slub_test_expect(void *obj, size_t size, uint64_t value) {
    volatile uint64_t *head = (volatile uint64_t *)obj;
    volatile uint64_t *tail =
        (volatile uint64_t *)((char *)obj + size - sizeof(uint64_t));

    assert(*head == value);
    assert(*tail == ~value);
}

// Case 1：每个 size class 分配、写入、释放、再分配复用
static void
slub_test_all_classes(void) {
    for (size_t i = 0; i < SLUB_NUM_CLASSES; i++) {
        size_t size = caches[i].object_size;
        uint64_t value = 0x1000 + i;
        void *first = slub_alloc(size);
        void *second;

        assert(first != NULL);
        assert(((uintptr_t)first & (SLUB_ALIGN - 1)) == 0);

        slub_test_fill(first, size, value);
        slub_free(first);

        // 释放后的对象在 freelist 头部，再分配应当拿回同一个对象，
        // 且用户数据没有被 freelist 指针覆盖。
        second = slub_alloc(size);
        assert(second == first);
        slub_test_expect(second, size, value);
        slub_free(second);
    }
}

// Case 2：多个 size class 的对象同时存活，互不覆盖
static void
slub_test_cross_class(void) {
    size_t sizes[5] = {16, 64, 256, 1024, 4096};
    void *objs[5];
    uint64_t marks[5];

    for (size_t i = 0; i < 5; i++) {
        objs[i] = slub_alloc(sizes[i]);
        assert(objs[i] != NULL);
        marks[i] = 0xA5A50000 + i;
    }

    // 地址区间两两不重叠
    for (size_t i = 0; i < 5; i++) {
        for (size_t j = i + 1; j < 5; j++) {
            uintptr_t a = (uintptr_t)objs[i];
            uintptr_t b = (uintptr_t)objs[j];
            assert(a + sizes[i] <= b || b + sizes[j] <= a);
        }
    }

    for (size_t i = 0; i < 5; i++) {
        slub_test_fill(objs[i], sizes[i], marks[i]);
    }

    // 释放其中一个，其余对象内容不受影响
    slub_free(objs[2]);
    for (size_t i = 0; i < 5; i++) {
        if (i != 2) {
            slub_test_expect(objs[i], sizes[i], marks[i]);
        }
    }

    for (size_t i = 0; i < 5; i++) {
        if (i != 2) {
            slub_free(objs[i]);
        }
    }
}

// Case 3：32 字节档耗尽 freelist，触发新 slab
static void
slub_test_new_slab(void) {
    struct slub_cache *cache = &caches[1];  // 32 字节
    size_t count = cache->objects_per_slab + 1;
    size_t pages_before = nr_free_pages();

    assert(count <= SLUB_TEST_MAX_OBJECTS);

    for (size_t i = 0; i < count; i++) {
        test_objs[i] = slub_alloc(cache->object_size);
        assert(test_objs[i] != NULL);
    }

    // 原有 freelist 已被耗尽，slub 向 pmm_manager 多申请了一个 slab
    assert(nr_free_pages() == pages_before - 1);

    for (size_t i = 0; i < count; i++) {
        slub_free(test_objs[i]);
    }

    // 简化实现不把空 slab 归还 pmm_manager，因此页数保持在减少后的值；
    // 所有对象都已回到 freelist，对象层面的状态已经恢复。
    assert(nr_free_pages() == pages_before - 1);
}

// Case 4：非法参数
static void
slub_test_boundary(void) {
    struct slub_header *freelist_before = caches[0].freelist;
    size_t pages_before = nr_free_pages();

    assert(slub_alloc(0) == NULL);
    assert(slub_alloc(8192) == NULL);

    slub_free(NULL);
    assert(caches[0].freelist == freelist_before);
    assert(nr_free_pages() == pages_before);
}

void
slub_check(void) {
    // 确保 cache 已初始化。启动阶段还没有分配过对象，这里重复初始化是安全的。
    slub_init();

    cprintf("slub_check: begin, free pages %lu\n",
            (unsigned long)nr_free_pages());

    // 先让每个 size class 各申请一个 slab 并归还对象。简化版 slub 不会把
    // slab 还给 pmm_manager，所以把这一步放在记录基线之前。
    for (size_t i = 0; i < SLUB_NUM_CLASSES; i++) {
        void *obj = slub_alloc(caches[i].object_size);
        assert(obj != NULL);
        slub_free(obj);
    }

    size_t pages_before = nr_free_pages();

    slub_test_all_classes();
    assert(nr_free_pages() == pages_before);

    slub_test_cross_class();
    assert(nr_free_pages() == pages_before);

    slub_test_new_slab();
    assert(nr_free_pages() == pages_before - 1);

    slub_test_boundary();

    cprintf("slub_check: all cases passed, free pages %lu\n",
            (unsigned long)nr_free_pages());
}
