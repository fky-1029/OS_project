#include <default_pmm.h>
#include <best_fit_pmm.h>
#include <buddy_system_pmm.h>
#include <defs.h>
#include <error.h>
#include <memlayout.h>
#include <mmu.h>
#include <pmm.h>
#include <sbi.h>
#include <stdio.h>
#include <string.h>
#include <riscv.h>
#include <dtb.h>

// virtual address of physical page array
struct Page *pages;
// amount of physical memory (in pages)
size_t npage = 0;
// the kernel image is mapped at VA=KERNBASE and PA=info.base
uint64_t va_pa_offset;
// memory starts at 0x80000000 in RISC-V
// DRAM_BASE defined in riscv.h as 0x80000000
const size_t nbase = DRAM_BASE / PGSIZE;

// virtual address of boot-time page directory
uintptr_t *satp_virtual = NULL;
// physical address of boot-time page directory
uintptr_t satp_physical;

// physical memory management
const struct pmm_manager *pmm_manager;


static void check_alloc_page(void);

// init_pmm_manager - initialize a pmm_manager instance
static void init_pmm_manager(void) {
    pmm_manager = &buddy_system_pmm_manager;
    cprintf("memory management: %s\n", pmm_manager->name);
    pmm_manager->init();
}

// init_memmap - call pmm->init_memmap to build Page struct for free memory
static void init_memmap(struct Page *base, size_t n) {
    pmm_manager->init_memmap(base, n);
}

// alloc_pages - call pmm->alloc_pages to allocate a continuous n*PAGESIZE
// memory
struct Page *alloc_pages(size_t n) {
    return pmm_manager->alloc_pages(n);
}

// free_pages - call pmm->free_pages to free a continuous n*PAGESIZE memory
void free_pages(struct Page *base, size_t n) {
    pmm_manager->free_pages(base, n);
}

// nr_free_pages - call pmm->nr_free_pages to get the size (nr*PAGESIZE)
// of current free memory
size_t nr_free_pages(void) {
    return pmm_manager->nr_free_pages();
}

//lab2 function1 2413671
static void page_init(void) {
    extern char end[];
    uint64_t mem_base = get_memory_base();
    uint64_t mem_size = get_memory_size();

    if (mem_base == 0 || mem_size == 0) {
        panic("page_init: missing or empty physical memory range");
    }
    if (mem_size > ~(uint64_t)0 - mem_base) {
        panic("page_init: physical memory range overflows");
    }
    if (mem_base != DRAM_BASE) {
        panic("page_init: memory base is incompatible with the fixed DRAM mapping");
    }

    uint64_t mem_end = mem_base + mem_size;
    uintptr_t managed_end = ROUNDDOWN(
        mem_end < PHYSICAL_MEMORY_END ? mem_end : PHYSICAL_MEMORY_END,
        PGSIZE);
    va_pa_offset = PHYSICAL_MEMORY_OFFSET;

    cprintf("Physical memory detected: [0x%016lx, 0x%016lx), size 0x%016lx\n",
            mem_base, mem_end, mem_size);
    if (managed_end <= KERNEL_BEGIN_PADDR ||
        (uintptr_t)end < KERNBASE || (uintptr_t)end > KERNTOP) {
        panic("page_init: physical memory cannot cover the kernel layout");
    }

    uintptr_t kernel_end_pa = PADDR((uintptr_t)end);
    if (kernel_end_pa > managed_end) {
        panic("page_init: kernel extends beyond manageable physical memory");
    }

    // npage is an absolute page-number bound; descriptors begin at nbase.
    npage = PPN(managed_end);
    size_t descriptor_count = npage - nbase;
    if (descriptor_count > ~(size_t)0 / sizeof(struct Page)) {
        panic("page_init: page-descriptor array size overflows");
    }
    size_t descriptor_bytes = descriptor_count * sizeof(struct Page);
    uintptr_t descriptors_pa = ROUNDUP(kernel_end_pa, PGSIZE);

    // Validate the whole array before writing even its first descriptor.
    if (descriptors_pa > managed_end ||
        descriptor_bytes > managed_end - descriptors_pa) {
        panic("page_init: not enough memory for page descriptors");
    }
    pages = (struct Page *)(descriptors_pa + va_pa_offset);
    for (size_t i = 0; i < descriptor_count; i++) {
        struct Page *p = &pages[i];
        p->flags = 0;
        p->property = 0;
        set_page_ref(p, 0);
        SetPageReserved(p);
    }

    // Keep the kernel, descriptors and alignment gaps out of the free area.
    uintptr_t free_begin = ROUNDUP(descriptors_pa + descriptor_bytes, PGSIZE);
    size_t free_count = 0;
    if (free_begin < managed_end) {
        free_count = (managed_end - free_begin) / PGSIZE;
        init_memmap(pa2page(free_begin), free_count);
    }

    cprintf("Manageable physical memory end: 0x%016lx, descriptors: %lu\n",
            managed_end, (unsigned long)descriptor_count);
    cprintf("Page descriptors: VA 0x%016lx, PA [0x%016lx, 0x%016lx)\n",
            (uintptr_t)pages, descriptors_pa, descriptors_pa + descriptor_bytes);
    cprintf("Initial free pages: %lu, aligned start PA 0x%016lx\n",
            (unsigned long)free_count, free_begin);
}

/* pmm_init - initialize the physical memory management */
void pmm_init(void) {
    // We need to alloc/free the physical memory (granularity is 4KB or other size).
    // So a framework of physical memory manager (struct pmm_manager)is defined in pmm.h
    // First we should init a physical memory manager(pmm) based on the framework.
    // Then pmm can alloc/free the physical memory.
    // Now the first_fit/best_fit/worst_fit/buddy_system pmm are available.
    init_pmm_manager();

    // detect physical memory space, reserve already used memory,
    // then use pmm->init_memmap to create free page list
    page_init();

    // use pmm->check to verify the correctness of the alloc/free function in a pmm
    check_alloc_page();

    extern char boot_page_table_sv39[];
    satp_virtual = (pte_t*)boot_page_table_sv39;
    satp_physical = PADDR(satp_virtual);
    cprintf("satp virtual address: 0x%016lx\nsatp physical address: 0x%016lx\n", satp_virtual, satp_physical);
}

static void check_alloc_page(void) {
    pmm_manager->check();
    cprintf("check_alloc_page() succeeded!\n");
}
