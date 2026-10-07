# AI交互提示词

> 各成员只记录自己在本实验中实际使用的提示词；最终提交前由成员 C 汇总补全。

## 成员 A：2413671-吕旭婧

1. “我已经阅读了 `kern/init/entry.S`、`kern/init/init.c` 和 `tools/kernel.ld`。请结合 RISC-V 调用约定，帮我核对对 `la sp, bootstacktop` 与 `tail kern_init` 的理解：它们分别做了什么、为什么要这样写，以及它们处于启动流程的哪一步？”
2. “我在 Ubuntu 22.04 虚拟机中配置好 RISC-V 工具链后，已经完成了 Lab1 的构建和启动。请告诉我应该查看哪些符号、反汇编和 QEMU 输出，来确认 `kern_entry` 的地址、内核栈位置以及是否进入了 `kern_init`。”
3. “我根据运行结果得到了 `kern_entry`、`bootstack`、`bootstacktop` 的地址，以及 `kern_entry` 的反汇编。请帮我检查这些结果能否支持练习 1 的结论，并帮我把从 QEMU/OpenSBI 到 `kern_entry`、`kern_init` 的过程整理成实验报告语言。”
4. “课程要求 QEMU 使用 4.1.1。我已在不替换原有环境的前提下单独安装该版本，并用原始 loader 启动参数重新运行 `make qemu`，看到了 OpenSBI 和 `os is loading ...` 输出。请帮我核对这一流程是否满足要求，以及报告中应该怎样记录运行命令和验证结果。”
