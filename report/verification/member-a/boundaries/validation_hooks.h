/* Test-copy hooks only. The page_init function body stays byte-for-byte intact. */
#include <test_lab2.h>

static uint64_t validation_memory_base(void);
static uint64_t validation_memory_size(void);
static void validation_init_memmap(struct Page *base, size_t n);
static void __noreturn validation_panic(const char *file, int line,
                                       const char *message, ...);

#define get_memory_base validation_memory_base
#define get_memory_size validation_memory_size
#define init_memmap validation_init_memmap
#undef panic
#define panic(...) validation_panic(__FILE__, __LINE__, __VA_ARGS__)
