# AI 交互提示词

## 2413671-吕旭婧 · 功能模块一

以下按发送顺序归档工程检查、实现和运行验证三轮用户提示词。

### 1. 工程检查

````text
[PROMPT：任务与操作要求]

我负责操作系统 Lab3 的模块一：Trap 上下文恢复与入口。组员已完成统筹，请以小组仓库现有的 Lab3 版本为基础，先检查工程，暂不实现代码。

本地指导书：
D:\lvxujing\Download\CodexHomework\操作系统\lab3\lab3：断,都可以断.txt

小组仓库：
https://github.com/fky-1029/OS_project.git

请明确核对 GitHub 上的 lab3 分支及提交号。这个仓库此前默认只跟踪 lab1，不能仅凭本地远端引用判断 lab3 是否最新。不要切换现有 Lab1 或 Lab2 的工作目录。

[RELY：已有条件]

模块一位于 kern/trap/trapentry.S，目标是 RESTORE_ALL 和 __alltraps。以实际源码中的 SAVE_ALL、struct trapframe、trap()、__trapret 以及 RISC-V 调用约定为依据。

现有 Ubuntu 实验环境可尝试通过 ssh ubuntu-vm 连接。先检查工具链和 QEMU 4.1.1 是否可用，不安装软件，不改变其他实验的环境。

[GUARANTEE：本轮交付]

给出当前 Lab3 的版本、模块一剩余工作、允许修改的位置、与模块二的依赖，以及可执行的验证方案。建议一个独立的 Lab3 开发目录。

[SPECIFICATION：检查重点]

1. 查看 RESTORE_ALL、__alltraps 及其标记位置，区分框架、组员已经完成的内容和我还需要补的内容。
2. 对照 SAVE_ALL 和 struct trapframe，说明通用寄存器与 CSR 的保存布局、sp 为什么要特殊处理，以及哪些字段决定 sret 后的状态。
3. 说明当前代码如何从汇编入口调用 trap()，以及如何衔接 __trapret。留意模块二对 tf->epc 的更新怎样影响返回位置。
4. 检查现有构建和测试入口，区分能单独验证模块一的检查与依赖模块二的集成测试。给出成功时应观察到的结果，不把尚未运行的测试写成通过。
5. 保留组员现有源码、报告和目录结构。本轮不修改文件、不提交、不推送，不执行含批量删除的清理或评分流程。
````

### 2. 模块一实现

````text
[PROMPT：任务与操作要求]

现在实现 Lab3 模块一：Trap 上下文恢复与入口。

请在独立的 Lab3 目录开发：
D:\lvxujing\Download\CodexHomework\操作系统\lab3\OS_project-lab3

小组仓库为 https://github.com/fky-1029/OS_project.git。先核对远端 lab3 的最新提交；上一轮检查基于 a4916fa。保留组员后续提交，不切换或覆盖现有 Lab1、Lab2 工作目录。如果模块一标记处已有别人新增的实现，先说明差异，不直接覆盖。

只补齐 code/kern/trap/trapentry.S 中 RESTORE_ALL 和 __alltraps 对应的两处标记。保留 lab3 标记，把本模块的 YOUR CODE 替换为我的学号 2413671。

本轮不修改模块二、构建配置或报告，不提交、不推送。

[RELY：已有接口与约束]

以实际工程中的 SAVE_ALL、struct trapframe、REGBYTES、LOAD/STORE 宏、trap(struct trapframe *tf)、__trapret 和 sret 为依据。

上一轮核对的 RV64 Trap frame 为 288 字节：x2 槽保存入 Trap 前的 sp，status 和 epc 分别位于 256、264 字节偏移。实现前再与当前源码核对，不能另定一套布局。

遵守 RISC-V 调用约定，保留现有 SAVE_ALL、结构体布局、trap() 签名和 __trapret。cause、badvaddr 是供处理与诊断使用的记录，不需要把它们当成 sret 的返回状态来恢复。

[GUARANTEE：交付内容]

1. 完成 RESTORE_ALL 与 __alltraps 的待实现部分。
2. 给出模块一的修改差异、单独汇编命令和反汇编检查结果。
3. 简要解释参数怎样传入 C 函数、C 函数返回到哪里，以及原 sp 和返回地址怎样恢复。
4. 明确列出尚未进行的运行验证。

[SPECIFICATION：行为与验收]

一、入口

SAVE_ALL 完成后，trap() 的第一个参数应指向当前 Trap frame。调用时保持栈对齐和帧的完整性。trap() 正常返回后，应进入现有 __trapret，且 sp 仍指向同一份帧。

二、恢复

恢复内容与 SAVE_ALL 的实际保存布局对应。将帧中当前的 status、epc 恢复到 sstatus、sepc；允许 epc 已由 C 处理函数更新。模块一不自行判断异常类型，不额外调整 epc，也不强制设置返回特权级。

恢复通用寄存器时，不能让写 CSR 使用的临时值覆盖最终寄存器状态。读取帧内其余内容完成后，再恢复帧中记录的原 sp。最终通过现有 sret 返回。

三、本轮验证

在 Ubuntu 中核对已有工具链。使用工程适用的包含路径和编译参数单独汇编 trapentry.S，将产物放在新验证目录，再用 objdump -dr 检查：

- 对 trap 的调用或对应重定位；
- 调用返回位置与 __trapret 的衔接；
- status、epc 槽与 CSR 写回；
- 最后恢复原 sp，再执行 sret。

保存实际命令和结果。单独汇编成功只说明模块一能汇编且产物符合上述检查，不能据此声称整核运行通过。

模块二尚未完成时，不为整核链接临时改写正式 handler。运行时 GDB 检查和评分用例留到下一轮。

如果 Ubuntu 尚未启动，先完成源码实现与静态核对，如实说明汇编验证待运行。不安装软件，不改环境，不执行含批量删除的清理或评分流程。
````

### 3. 运行验证

````text
[PROMPT：任务与操作要求]

模块一的两处代码已实现，单独汇编和反汇编检查通过。现在验证实际发生 Trap 时，入口传参、上下文恢复和返回是否正确。

继续以当前 Lab3 开发目录为准：
D:\lvxujing\Download\CodexHomework\操作系统\lab3\OS_project-lab3

请在 Ubuntu 上创建独立测试副本，使用当前实际的 trapentry.S。模块二尚未合入，本轮可以在测试副本中加最小的断点处理和测试入口，但不能把它们混入正式源码，也不替组员实现模块二。

本轮不整理最终报告、不提交、不推送。

[RELY：已有条件]

- 当前实现只改了 trapentry.S 的两个模块一位置。
- SAVE_ALL 保存通用寄存器和 CSR，Trap frame 在 RV64 下为 288 字节。
- __alltraps 调用 trap(struct trapframe *tf)，C 返回后由 __trapret 和 sret 恢复执行。
- 现有环境使用 ~/compiler-lab/env.sh 和已有 QEMU 4.1.1。
- 新版汇编器不接受框架的 sbadaddr。沿用已验证的命令行兼容宏 -Dsbadaddr=stval，优先通过 Makefile 已有的 DEFS 参数传入，不改正式 SAVE_ALL 或 Makefile。

[GUARANTEE：交付内容]

1. 独立测试副本中的实际构建和 QEMU/GDB 验证结果。
2. 证明参数传递、栈指针恢复、代表性寄存器恢复和返回地址正确的输出。
3. 可复现命令、原始日志，以及测试副本相对正式源码的改动说明。
4. 列出仍需模块二配合完成的集成测试。

[SPECIFICATION：验证场景与标准]

在测试副本中，使用一条长度已知的断点指令触发 Trap。最小处理函数只需要识别这次断点并将 tf->epc 指向预期的下一条指令。隔离时钟中断，避免其他 Trap 干扰。记录实际指令编码或反汇编，不假定所有断点指令都是同一长度。

用 GDB 或等效的运行时检查确认：

1. 进入 __alltraps、执行 SAVE_ALL 前，记录原 sp。保存完成后，帧内 x2 槽应等于原 sp。
2. 在 trap() 第一条指令处检查 a0，确认它等于汇编传入的帧地址。注意不要在 C 函数已经调整栈之后才比较 sp。
3. C 函数返回到 __trapret 时，sp 应仍指向同一份帧，并能观察到处理函数修改后的 tf->epc。
4. 执行 sret 前，确认 sepc 等于更新后的地址，原 sp 已恢复。选取能区分临时值和原值的测试数据，核对 a0、t0、ra 等代表性寄存器；明确记录实际核对了哪些。
5. 单步执行 sret 后，PC 应落在预期地址，后续代码能够继续执行，不重复触发同一断点。区分 sret 前恢复的 sstatus 与 sret 指令自身对状态位的更新，不要求两者按位相同。

核对测试副本中的模块一实现与正式版本一致。若遇到构建或启动问题，先说明它属于工具链、框架、测试驱动还是模块一；如需改正式代码，只在模块一范围内做最小修正并保留前后证据。

禁止批量删除，不执行带批量清理的 clean 或 grade 流程。不修改正式模块二、测试断言或报告。独立验证通过只代表模块一的断点返回路径通过，不宣称时钟、非法指令或完整评分通过。
````


## 2413396-范珂祎 · 功能模块二

模块二的核心逻辑一次实现，首轮迭代因漏 `#include <sbi.h>` 报隐式声明错误，修复后即为最终版本。以下为最终成功实现该功能的提示词，已把 `sbi.h` 依赖、`-Dsbadaddr=stval` 构建参数和评分脚本输出要求提前写入 [RELY]/[SPECIFICATION]。

````text
[PROMPT：任务与操作要求]

我负责操作系统 Lab3 的模块二：中断与异常处理功能。组员已完成模块一（trapentry.S 的 RESTORE_ALL 与 __alltraps）与统筹，请以小组仓库当前 lab3 分支为基础，实现 kern/trap/trap.c 中的 interrupt_handler 与 exception_handler 两个函数。

小组仓库：
https://github.com/fky-1029/OS_project.git

只修改 code/kern/trap/trap.c 中 "lab3 function2 YOUR CODE" 标记处。保留 lab3 标记，把 YOUR CODE 替换为我的学号 2413396。不修改 trapentry.S、struct trapframe、trap()/trap_dispatch() 等框架与组员已有代码，不修改函数签名，不把两个函数改成 static（tools/test_lab3.c 会 extern 引用它们）。

[RELY：已有条件]

以实际工程源码为准，不要凭记忆描述：

- struct trapframe 布局见 kern/trap/trap.h：gpr（32 个通用寄存器）+ status + epc + badvaddr + cause。中断/异常分发入口 trap_dispatch() 已按 (intptr_t)tf->cause < 0 区分中断与异常，分别调用 interrupt_handler / exception_handler。
- libs/riscv.h 定义 IRQ_U_SOFT=0、IRQ_S_SOFT=1、IRQ_H_SOFT=2、IRQ_M_SOFT=3、IRQ_U_TIMER=4、IRQ_S_TIMER=5、IRQ_H_TIMER=6、IRQ_M_TIMER=7、IRQ_U_EXT=8、IRQ_S_EXT=9、IRQ_H_EXT=10、IRQ_M_EXT=11，以及 CAUSE_ILLEGAL_INSTRUCTION=0x2、CAUSE_BREAKPOINT=0x3。
- kern/driver/clock.h 提供 volatile size_t ticks 与 void clock_set_next_event(void)。
- libs/sbi.h 提供 void sbi_shutdown(void)。注意 trap.c 需要 #include <sbi.h>，否则编译报隐式声明错误。
- trap.c 顶部已定义 #define TICK_NUM 100 与 static void print_ticks()（打印 "100 ticks"）。
- kern/debug/kdebug.h 提供 void print_trapframe(struct trapframe *tf)。
- 新工具链需要 make DEFS=-Dsbadaddr=stval 编译（模块一同样如此）。

[GUARANTEE：本轮交付]

1. 实现 interrupt_handler 与 exception_handler，且能通过 make DEFS=-Dsbadaddr=stval 编译。
2. 给出 tools/grade.sh 三个测试用例（GRADE_LAB3_CONTEXT / GRADE_LAB3_ILLEGAL / GRADE_LAB3_TIMER）对应的输出要求，并说明实现如何满足。
3. 说明时钟中断、非法指令、断点各自的处理流程，以及 tf->epc 如何更新。

[SPECIFICATION：行为与验收]

一、interrupt_handler

- 先屏蔽 tf->cause 的最高位得到真实中断号（用 (tf->cause << 1) >> 1）。
- IRQ_S_TIMER：先 clock_set_next_event() 重设下一次时钟事件；ticks 每次加一；ticks % TICK_NUM == 0 时调用 print_ticks()；累计输出达到 SHUTDOWN_AFTER 次后调用 sbi_shutdown() 关机。SHUTDOWN_AFTER 本实验取 10，保证评分用例的 ticks 能跑到 200 而不会提前关机。
- IRQ_S_SOFT：输出包含 "Supervisor software interrupt" 的信息；其余软中断/定时器中断做类型区分提示。
- 无法识别的中断：输出包含 "Unknown interrupt: cause <号>" 的信息，并调用 print_trapframe(tf)。

二、exception_handler

- CAUSE_ILLEGAL_INSTRUCTION：输出包含 "Illegal instruction at" 的信息；CAUSE_BREAKPOINT：输出包含 "Breakpoint at" 的信息。
- 两者都要把 tf->epc 移动到下一条指令：读 tf->epc 处的 16 位指令，低两位 == 0b11 说明是 32 位指令则 epc += 4，否则是 16 位压缩指令则 epc += 2。
- 其余异常调用 print_trapframe(tf) 兜底。

三、验收

- 三个评分用例分别以 DEFS=-DGRADE_LAB3_CONTEXT / -DGRADE_LAB3_ILLEGAL / -DGRADE_LAB3_TIMER 编译运行，通过 tools/grade.sh 中全部 grep 检查（关键输出串：Breakpoint at、Illegal instruction at、Supervisor software interrupt、Unknown interrupt: cause 15、cause 0x00000014、100 ticks、ticks=200）。
````
