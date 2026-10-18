/* Included after the real page_init, in the verification copy only. */
#undef get_memory_base
#undef get_memory_size
#undef init_memmap
#undef panic
#define panic(...) __panic(__FILE__, __LINE__, __VA_ARGS__)

extern char end[];
extern uint64_t sbi_call(uint64_t, uint64_t, uint64_t, uint64_t);

static unsigned int validation_case;
static uint64_t validation_base, validation_size;
static const char *validation_expected_error;
static uintptr_t validation_descriptor_start;
static size_t validation_probe_bytes;
static size_t validation_calls, validation_registered_count;
static struct Page *validation_registered_base;
static bool validation_no_free;

static unsigned char *validation_pointer(uintptr_t physical) {
    return (unsigned char *)(physical + PHYSICAL_MEMORY_OFFSET);
}

static void validation_fill(uintptr_t start, size_t length) {
    volatile unsigned char *p = validation_pointer(start);
    for (size_t i = 0; i < length; i++) p[i] = 0xa5;
}

static void validation_check_sentinel(uintptr_t start, size_t length) {
    volatile unsigned char *p = validation_pointer(start);
    for (size_t i = 0; i < length; i++) {
        assert(p[i] == 0xa5);
    }
}

static uint64_t validation_memory_base(void) { return validation_base; }
static uint64_t validation_memory_size(void) { return validation_size; }

static uintptr_t validation_limit(void) {
    uint64_t top = validation_base + validation_size;
    if (top > PHYSICAL_MEMORY_END) top = PHYSICAL_MEMORY_END;
    return top - top % PGSIZE;
}

static void validation_prepare(void) {
    validation_case = *(volatile unsigned int *)validation_pointer(0x81000000);
    validation_base = get_memory_base();
    validation_size = get_memory_size();
    validation_descriptor_start = ROUNDUP(
        (uintptr_t)end - PHYSICAL_MEMORY_OFFSET, PGSIZE);
    uintptr_t real_top = get_memory_base() + get_memory_size();
    if (real_top > PHYSICAL_MEMORY_END) real_top = PHYSICAL_MEMORY_END;
    validation_probe_bytes = (PPN(real_top) - nbase) * sizeof(struct Page) + 32;
    assert(validation_descriptor_start + validation_probe_bytes < real_top);

    switch (validation_case) {
    case 0: case 1: case 2:  /* Real DTB input, 128 / 64 / 256 MiB RAM. */
        break;
    case 10:
        validation_base = 0;
        validation_expected_error = "page_init: missing or empty physical memory range";
        break;
    case 11:
        validation_size = 0;
        validation_expected_error = "page_init: missing or empty physical memory range";
        break;
    case 12:
        validation_size = ~(uint64_t)0;
        validation_expected_error = "page_init: physical memory range overflows";
        break;
    case 13:
        validation_base = DRAM_BASE + PGSIZE;
        validation_expected_error = "page_init: memory base is incompatible with the fixed DRAM mapping";
        break;
    case 14:
        validation_size = KERNEL_BEGIN_PADDR - DRAM_BASE;
        validation_expected_error = "page_init: physical memory cannot cover the kernel layout";
        break;
    case 15:
        validation_size = (uintptr_t)end - PHYSICAL_MEMORY_OFFSET - 1 - DRAM_BASE;
        validation_expected_error = "page_init: kernel extends beyond manageable physical memory";
        break;
    case 16:
        validation_size = validation_descriptor_start + PGSIZE - DRAM_BASE;
        validation_expected_error = "page_init: not enough memory for page descriptors";
        break;
    case 17: case 18: {
        /* Search for an input boundary, not an alternative initialization. */
        uintptr_t boundary;
        for (boundary = validation_descriptor_start + PGSIZE;
             boundary < validation_descriptor_start + 64 * PGSIZE;
             boundary += PGSIZE) {
            uintptr_t array_end = validation_descriptor_start +
                (PPN(boundary) - nbase) * sizeof(struct Page);
            if (array_end <= boundary && ROUNDUP(array_end, PGSIZE) == boundary) {
                validation_size = boundary - DRAM_BASE +
                    (validation_case == 17 ? 1 : 0);
                assert(validation_base + validation_size - array_end < PGSIZE);
                validation_no_free = 1;
                break;
            }
        }
        assert(validation_no_free);
        break;
    }
    case 19:
        validation_size = 64 * 1024 * 1024UL + 123;
        break;
    default:
        panic("validation: unknown scenario");
    }

    validation_fill(validation_descriptor_start, validation_probe_bytes);
    uintptr_t kernel_tail = (uintptr_t)end - PHYSICAL_MEMORY_OFFSET;
    validation_fill(kernel_tail, validation_descriptor_start - kernel_tail);
    if (!validation_expected_error) {
        uintptr_t array_end = validation_descriptor_start +
            (PPN(validation_limit()) - nbase) * sizeof(struct Page);
        validation_fill(array_end, 32);
    }
    cprintf("[Driver] INPUT id=%u mode=%s real_base=0x%016lx real_size=0x%016lx base=0x%016lx size=0x%016lx kernel_end_pa=0x%016lx\n",
            validation_case, validation_case <= 2 ? "DTB" : "injected",
            get_memory_base(), get_memory_size(), validation_base,
            validation_size, kernel_tail);
}

static void validation_init_memmap(struct Page *base, size_t n) {
    assert(validation_expected_error == NULL);
    assert(!validation_no_free);
    assert(base >= pages && n > 0);
    assert((size_t)(base - pages) < npage - nbase);
    assert(n <= npage - nbase - (size_t)(base - pages));
    uintptr_t start = page2pa(base);
    uintptr_t array_end = validation_descriptor_start +
        (npage - nbase) * sizeof(struct Page);
    assert(start >= ROUNDUP(array_end, PGSIZE));
    assert(start + n * PGSIZE <= validation_limit());
    assert(++validation_calls == 1);
    validation_registered_base = base;
    validation_registered_count = n;
    init_memmap(base, n); /* Unchanged First-Fit initialization. */
}

static void __noreturn validation_panic(const char *file, int line,
                                       const char *message, ...) {
    assert(validation_expected_error != NULL);
    assert(strcmp(message, validation_expected_error) == 0);
    assert(validation_calls == 0);
    assert(pages == NULL);
    assert(nr_free_pages() == 0);
    validation_check_sentinel(validation_descriptor_start, validation_probe_bytes);
    uintptr_t tail = (uintptr_t)end - PHYSICAL_MEMORY_OFFSET;
    validation_check_sentinel(tail, validation_descriptor_start - tail);
    cprintf("[Driver] REJECTION_CHECK id=%u calls=0 pages=NULL candidate_and_guards_unchanged=1 bytes=%lu\n",
            validation_case, (unsigned long)validation_probe_bytes);
    cprintf("[Driver] EXPECTED_REJECTION id=%u message=%s\n", validation_case, message);
    __panic(file, line, "%s", message); /* Preserve the real non-returning panic. */
}

static void validation_check_layout(void) {
    assert(validation_expected_error == NULL);
    assert(va_pa_offset == PHYSICAL_MEMORY_OFFSET);
    assert(npage == PPN(validation_limit()));
    assert((uintptr_t)pages == ROUNDUP((uintptr_t)end, PGSIZE));
    uintptr_t array_end = validation_descriptor_start +
        (npage - nbase) * sizeof(struct Page);
    assert(array_end <= validation_limit());
    uintptr_t free_start = ROUNDUP(array_end, PGSIZE);
    size_t free_expected = (validation_limit() - free_start) / PGSIZE;
    assert(nr_free_pages() == free_expected);
    assert(validation_calls == (free_expected ? 1 : 0));
    if (free_expected) {
        assert(page2pa(validation_registered_base) == free_start);
        assert(validation_registered_count == free_expected);
        assert(PageProperty(validation_registered_base));
        assert(validation_registered_base->property == free_expected);
    }
    for (size_t i = 0; i < npage - nbase; i++) {
        struct Page *p = &pages[i];
        uintptr_t physical = DRAM_BASE + i * PGSIZE;
        assert(page2pa(p) == physical);
        assert(pa2page(physical) == p);
        assert(page_ref(p) == 0);
        assert(!!PageReserved(p) == (physical < free_start));
        if (p != validation_registered_base) assert(!PageProperty(p));
    }
    validation_check_sentinel(array_end, 32);
    uintptr_t tail = (uintptr_t)end - PHYSICAL_MEMORY_OFFSET;
    validation_check_sentinel(tail, validation_descriptor_start - tail);
    cprintf("[Driver] LAYOUT_PASS id=%u npage=0x%lx descriptors=%lu free=%lu calls=%lu free_start=0x%016lx managed_end=0x%016lx guards_unchanged=1\n",
            validation_case, (unsigned long)npage, (unsigned long)(npage - nbase),
            (unsigned long)free_expected, (unsigned long)validation_calls,
            free_start, validation_limit());
    if (validation_case == 0) test_page_init();
    if (validation_no_free) {
        assert(free_expected == 0);
        cprintf("[Driver] NO_FREE_PASS id=%u init_memmap_calls=0\n", validation_case);
    }
}

static void validation_finish(void) {
    cprintf("[Driver] SCENARIO_PASS id=%u\n", validation_case);
    sbi_call(8, 0, 0, 0); /* Test-only shutdown after checks have actually run. */
}
