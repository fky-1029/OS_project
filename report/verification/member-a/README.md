# 成员 A 的验证材料

本目录只保留支撑报告结论的命令、原始日志、测试驱动及必要依赖。不包含完整测试工程副本、obj/bin 编译产物或整个历史运行目录。

## 内容与证据

- `first-run/`：正式实现的首次构建、运行、命令记录、文件哈希及当时执行的脚本。
- `boundaries/`：原专项与边界测试的生成脚本 `run_suite.py`，以及依赖的 `validation_hooks.h`、`validation_driver.inc`。
- `boundaries/logs/`：原始构建输出、13 个场景的日志、真实命令、QEMU 版本、逐项结果和哈希清单。
- [screenshots.md](./screenshots.md)：两张已完成真实截图的路径，以及可选截图的回看命令。
- [reproduce.sh](./reproduce.sh)：适用于当前仓库布局的复现入口。它是材料整理时增加的便携入口，不是历史测试的原始命令记录。

首次实际运行的关键证据是 [run.log](./first-run/run.log) 中的 `check_alloc_page() succeeded!` 和 31929 个空闲页。专项默认场景的 [default-128MiB.log](./boundaries/logs/default-128MiB.log) 实际包含 `[Test 1] Page Init: PASS`，空闲页为 31928；驱动增大内核后，描述符起点向后移动了一页。这两份日志来自不同镜像，数字不能合并为同一次运行结果。

已补入两张原样保存的真实终端截图：[首次构建与运行日志回看](../../images/member-a-lab2-first-run.png)、[模块一专项测试日志回看](../../images/member-a-lab2-page-init-test.png)，分别引用于报告 5.1、5.2 节。它们展示历史运行日志，不是重新运行实验。成员 A 的工具为 Codex Desktop，模型为 GPT-6，本次实验未更换模型。

13 个场景的表格和每项日志入口见 [results.md](./boundaries/logs/results.md)，精确输入、退出码和证据见 [results.json](./boundaries/logs/results.json)。异常场景中出现的 panic 是预期拒绝行为，不是初始化成功标记；是否通过还检查了错误信息、调用次数和哨兵。无空闲页场景没有调用原有分配成功检查。

`manifest.json` 保存函数体、原测试文件和正式源码前后哈希。测试生成器保留了实际 `page_init()` 函数体，只在测试副本中通过预处理宏接入输入读取、注册计数和 panic 观察。默认场景调用未修改的 `test_page_init()`；256 MiB 和无空闲页场景使用独立检查，未改写原测试来适配。所有场景在 QEMU 4.1.1 中运行，不是在主机上重写初始化算法。

## 复现方法

前提：将当前仓库放在 Ubuntu 的任意目录，保持仓库根目录下的 `code/` 与 `report/` 相对关系。需要已有 RISC-V 工具链、Make、Python 3、timeout 和 QEMU 4.1.1。Python 脚本仅使用标准库。

在仓库根目录执行：

```bash
# 只构建并运行正式模块一实现，保持 First-Fit。
bash report/verification/member-a/reproduce.sh first

# 原样接入专项测试，再运行 13 个场景。
bash report/verification/member-a/reproduce.sh boundaries
```

默认复用当前机器的 `~/compiler-lab/env.sh` 载入工具链，并使用 `~/os-lab/tools/qemu-4.1.1/bin/qemu-system-riscv64`。其他机器可以显式传入路径，例如：

```bash
LAB2_ENV=/absolute/path/to/toolchain-env.sh \
LAB2_QEMU=/absolute/path/to/qemu-system-riscv64 \
bash report/verification/member-a/reproduce.sh boundaries
```

两种模式都在 `~/os-lab/lab2-a-repro-XXXXXXXX` 新建验证目录，保留生成副本和日志，不删除旧目录，也不修改正式源码。请在原有材料对应的源码版本上复现；加入其他成员代码后，内核大小、分配器选择和空闲页数可能改变，不能据此直接否定历史结果。

历史原始命令见 [首次 session.log](./first-run/session.log) 和 [专项 commands.log](./boundaries/logs/commands.log)。首次的 `verify-first-run.sh` 保留原机器绝对路径，仅用于核对当时执行过程；跨目录复现应使用上述便携入口。

材料整理阶段只对便携入口做语法、路径和依赖检查，没有把这些检查写成又一轮 QEMU 验证。原来的 13 场景结果来自已保留的实际运行日志。

## 范围与限制

本轮场景检查了内存范围、描述符位置、逐页状态、注册区间、地址转换和部分错误处理。无效输入场景监控描述符候选区、内核末端对齐区及相关哨兵。静态检查补充核对了容量验证先于写入、循环上界及对齐计算；这不是全部输入和任意内存访问的形式化证明。

Best-Fit、Buddy、完整 Lab2 评分、损坏 DTB 字节的解析和其他平台映射没有在本材料中验证。没有执行 `clean`、`grade` 中的批量删除命令。
