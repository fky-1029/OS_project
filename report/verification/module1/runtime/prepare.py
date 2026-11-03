from pathlib import Path

code = Path("code")
init = code / "kern/init/init.c"
trap = code / "kern/trap/trap.c"
probe = code / "kern/init/lab3_module1_probe.S"

def replace_once(text, old, new):
    if text.count(old) != 1:
        raise SystemExit(f"Expected one match for {old!r}")
    return text.replace(old, new, 1)

if probe.exists():
    raise SystemExit("Prepare a fresh test copy before injecting the probe")

init_text = replace_once(init.read_text(), "#include <dtb.h>", """#include <dtb.h>
#include <assert.h>
#include <riscv.h>

extern uintptr_t lab3_module1_probe(void);
extern volatile unsigned int lab3_module1_trap_count;

__attribute__((noreturn, noinline, noclone))
void lab3_module1_finished(void) {
    while (1) {
        asm volatile("nop");
    }
}""")
init_text = replace_once(init_text, """    clock_init();   // init clock interrupt
    intr_enable();  // enable irq interrupt

    /* do nothing */
    while (1)
        ;""", """    intr_disable();
    write_csr(sie, 0);
    intr_enable();  // Exercise SPIE -> SIE while every interrupt source is masked.
    uintptr_t probe_result = lab3_module1_probe();
    assert(probe_result == 0x3579c);
    assert(lab3_module1_trap_count == 1);
    cprintf("LAB3_MODULE1_PROBE_PASS traps=%d result=0x%lx\\n",
            lab3_module1_trap_count, (unsigned long)probe_result);
    lab3_module1_finished();""")

trap_text = trap.read_text()
marker = "//lab3 function2 YOUR CODE"
if trap_text.count(marker) != 1 or "void trap(struct trapframe *tf)" not in trap_text:
    raise SystemExit("Unexpected trap.c skeleton")
trap_text = trap_text.split(marker, 1)[0] + """/* Test-only breakpoint handler. */
extern char lab3_module1_breakpoint[], lab3_module1_resume[];
volatile unsigned int lab3_module1_trap_count;

void trap(struct trapframe *tf) {
    assert(tf->cause == CAUSE_BREAKPOINT);
    assert(tf->epc == (uintptr_t)lab3_module1_breakpoint);
    assert(lab3_module1_trap_count == 0);
    lab3_module1_trap_count++;
    tf->epc = (uintptr_t)lab3_module1_resume;
}
"""
init.write_text(init_text)
trap.write_text(trap_text)
probe.write_text("""    .text
    .align 2
    .globl lab3_module1_probe
    .type lab3_module1_probe, @function
lab3_module1_probe:
    addi sp, sp, -16
    sd ra, 8(sp)
    li ra, 0x13579
    li t0, 0x2468a
    li a0, 0x3579b
    .balign 4
    .globl lab3_module1_breakpoint
lab3_module1_breakpoint:
    .4byte 0x00100073
    .globl lab3_module1_resume
lab3_module1_resume:
    addi a0, a0, 1
    ld ra, 8(sp)
    addi sp, sp, 16
    ret
    .size lab3_module1_probe, .-lab3_module1_probe
""")
