#include <error.h>
#include <pmm.h>
#include <stdio.h>

// Called only after this branch's test mappings have all been removed.
static void release_test_tables(uintptr_t la)
{
    pde_t *root = &boot_pgdir_va[PDX1(la)];
    struct Page *middle_page = pde2page(*root);
    pde_t *middle = page2kva(middle_page);
    struct Page *leaf_page = pde2page(middle[PDX0(la)]);

    *root = 0;
    asm volatile("sfence.vma" ::: "memory");
    free_page(leaf_page);
    free_page(middle_page);
}

static void check_permissions_replacement_and_removal(void)
{
    const uintptr_t la = 0x4000;
    const uintptr_t alias_la = la + PGSIZE;
    const uintptr_t absent_root_la = 1UL << PDX1SHIFT;
    const uintptr_t absent_child_la = 1UL << PDX0SHIFT;
    const uint32_t rw = PTE_R | PTE_W;
    const pte_t permissions = PTE_V | PTE_R | PTE_W | PTE_X | PTE_U;
    const uint32_t old_value = 0x11223344;
    const uint32_t new_value = 0xaabbccdd;
    volatile uint32_t *target = (volatile uint32_t *)la;
    volatile uint32_t *alias = (volatile uint32_t *)alias_la;
    size_t initial_free = nr_free_pages();
    assert(boot_pgdir_va[PDX1(la)] == 0);
    assert(boot_pgdir_va[PDX1(absent_root_la)] == 0);

    struct Page *old_page = alloc_page();
    struct Page *new_page = alloc_page();
    assert(old_page != NULL && new_page != NULL && old_page != new_page);
    *(uint32_t *)page2kva(new_page) = new_value;

    assert(page_insert(boot_pgdir_va, old_page, la, rw) == 0);
    pte_t *ptep = get_pte(boot_pgdir_va, la, 0);
    assert(ptep != NULL && (*ptep & permissions) == (PTE_V | rw));
    assert(pte2page(*ptep) == old_page && page_ref(old_page) == 1);
    *target = old_value;

    size_t before = nr_free_pages();
    assert(page_insert(boot_pgdir_va, old_page, la, PTE_R) == 0);
    assert(get_pte(boot_pgdir_va, la, 0) == ptep);
    assert((*ptep & permissions) == (PTE_V | PTE_R));
    assert(pte2page(*ptep) == old_page && page_ref(old_page) == 1);
    assert(nr_free_pages() == before);
    cprintf("[module1] same-page PTE: W cleared, PPN unchanged, ref=1, free delta=0\n");

    uint32_t read_after_downgrade = *target;
    assert(read_after_downgrade == old_value);
    cprintf("[module1] same-page CPU read after downgrade: 0x%08x\n",
            read_after_downgrade);

    assert(page_insert(boot_pgdir_va, old_page, la, rw) == 0);
    assert(page_insert(boot_pgdir_va, old_page, alias_la, rw) == 0);
    assert(page_ref(old_page) == 2 && page_ref(new_page) == 0);
    uint32_t before_replace = *target;
    assert(before_replace == old_value);
    before = nr_free_pages();

    assert(page_insert(boot_pgdir_va, new_page, la, rw) == 0);
    uint32_t after_replace = *target;
    uint32_t through_alias = *alias;
    assert(after_replace == new_value && through_alias == old_value);
    assert((*ptep & permissions) == (PTE_V | rw));
    assert(pte2page(*ptep) == new_page && page_ref(new_page) == 1);
    assert(page_ref(old_page) == 1 && nr_free_pages() == before);
    cprintf("[module1] replacement CPU reads: target 0x%08x -> 0x%08x, alias 0x%08x\n",
            before_replace, after_replace, through_alias);

    pde_t root_entry = boot_pgdir_va[PDX1(la)];
    struct Page *middle_page = pde2page(root_entry);
    pde_t *middle = page2kva(middle_page);
    pde_t middle_entry = middle[PDX0(la)];
    pte_t *alias_ptep = get_pte(boot_pgdir_va, alias_la, 0);
    assert(alias_ptep != NULL && pte2page(*alias_ptep) == old_page);
    page_remove(boot_pgdir_va, alias_la);
    assert(*alias_ptep == 0 && page_ref(old_page) == 0);
    assert(nr_free_pages() == before + 1);
    cprintf("[module1] last old-page alias removed: PTE=0, old ref=0, free delta=+1\n");

    page_remove(boot_pgdir_va, la);
    assert(*ptep == 0 && page_ref(new_page) == 0);
    assert(nr_free_pages() == before + 2);
    assert(boot_pgdir_va[PDX1(la)] == root_entry);
    assert(middle[PDX0(la)] == middle_entry);
    assert(page_ref(middle_page) == 1 && page_ref(pde2page(middle_entry)) == 1);
    assert(get_pte(boot_pgdir_va, la, 0) == ptep);
    cprintf("[module1] valid deletion: PTE=0, new ref=0, free delta=+1, tables retained\n");

    before = nr_free_pages();
    page_remove(boot_pgdir_va, la);
    assert(*ptep == 0 && page_ref(new_page) == 0 && nr_free_pages() == before);
    assert(boot_pgdir_va[PDX1(la)] == root_entry && middle[PDX0(la)] == middle_entry);
    cprintf("[module1] repeated deletion: PTE=0, ref=0, free delta=0\n");

    assert(middle[PDX0(absent_child_la)] == 0);
    page_remove(boot_pgdir_va, absent_root_la);
    page_remove(boot_pgdir_va, absent_child_la);
    assert(boot_pgdir_va[PDX1(absent_root_la)] == 0);
    assert(boot_pgdir_va[PDX1(la)] == root_entry);
    assert(middle[PDX0(absent_child_la)] == 0 && middle[PDX0(la)] == middle_entry);
    assert(get_pte(boot_pgdir_va, absent_root_la, 0) == NULL);
    assert(get_pte(boot_pgdir_va, absent_child_la, 0) == NULL);
    assert(page_ref(old_page) == 0 && page_ref(new_page) == 0 && nr_free_pages() == before);
    cprintf("[module1] deletion with missing root or child: no new tables, free delta=0\n");

    release_test_tables(la);
    assert(nr_free_pages() == initial_free);
}

static const struct pmm_manager *real_manager;
static size_t allocation_count;
static size_t failure_index;

static struct Page *counted_alloc_pages(size_t n)
{
    assert(n == 1);
    allocation_count++;
    if (allocation_count == failure_index)
    {
        return NULL;
    }
    return real_manager->alloc_pages(n);
}

static int insert_with_allocation_probe(struct Page *page, uintptr_t la, size_t fail_at)
{
    real_manager = pmm_manager;
    struct pmm_manager probe = *real_manager;
    probe.alloc_pages = counted_alloc_pages;
    allocation_count = 0;
    failure_index = fail_at;
    pmm_manager = &probe;
    int result = page_insert(boot_pgdir_va, page, la, PTE_R | PTE_W);
    pmm_manager = real_manager;
    return result;
}

static void check_allocation_failure_and_retry(void)
{
    const uintptr_t la = 2UL << PDX1SHIFT;
    pde_t *root = &boot_pgdir_va[PDX1(la)];
    size_t initial_free = nr_free_pages();
    struct Page *page = alloc_page();
    assert(page != NULL && page_ref(page) == 0 && *root == 0);
    size_t before = nr_free_pages();

    int result = insert_with_allocation_probe(page, la, 1);
    assert(result == -E_NO_MEM && allocation_count == 1);
    assert(*root == 0 && get_pte(boot_pgdir_va, la, 0) == NULL);
    assert(page_ref(page) == 0 && nr_free_pages() == before);
    cprintf("[module1] OOM at allocation 1: rc=%d, no table or mapping, ref=0, free delta=0\n",
            result);

    result = insert_with_allocation_probe(page, la, 2);
    assert(result == -E_NO_MEM && allocation_count == 2);
    pde_t saved_root_entry = *root;
    assert(saved_root_entry & PTE_V);
    assert(get_pte(boot_pgdir_va, la, 0) == NULL);
    struct Page *middle_page = pde2page(saved_root_entry);
    pde_t *middle = page2kva(middle_page);
    for (size_t i = 0; i < NPTEENTRY; i++)
    {
        assert(middle[i] == 0);
    }
    assert(page_ref(middle_page) == 1 && page_ref(page) == 0);
    assert(nr_free_pages() == before - 1);
    cprintf("[module1] OOM at allocation 2: rc=%d, empty table retained at PA=0x%lx, "
            "data ref=0, free delta=-1\n", result, page2pa(middle_page));

    assert(insert_with_allocation_probe(page, la, 0) == 0);
    assert(allocation_count == 1 && *root == saved_root_entry);
    pte_t *ptep = get_pte(boot_pgdir_va, la, 0);
    assert(ptep != NULL && pte2page(*ptep) == page && page_ref(page) == 1);
    assert(page_ref(middle_page) == 1 && nr_free_pages() == before - 2);
    cprintf("[module1] OOM retry: same upper table reused, one allocation, data ref=1\n");

    page_remove(boot_pgdir_va, la);
    assert(*ptep == 0 && page_ref(page) == 0 && *root == saved_root_entry);
    release_test_tables(la);
    assert(nr_free_pages() == initial_free);
}

void module1_extra_tests(void)
{
    check_permissions_replacement_and_removal();
    check_allocation_failure_and_retry();
    cprintf("[module1] all extra assertions completed; free page count restored\n");
}
