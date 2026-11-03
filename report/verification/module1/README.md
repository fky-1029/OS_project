# 模块一验证材料

这些日志来自 2413671-吕旭婧的模块一验证。汇编和运行阶段对应的 `trapentry.S` SHA-256 均为 `a4f2075245ab4060b85554d79191549a0fd4ac2715595ee510d382c6310cf464`。

| 阶段 | 核对材料 |
|---|---|
| 单独汇编 | [实际命令](./assembly/verify.sh) · [未加兼容宏的日志](./assembly/verification-default.log) · [成功日志](./assembly/verification.log) · [对象文件反汇编](./assembly/trapentry.objdump.txt) · [模块一实现差异](./assembly/module1.diff) |
| 测试副本构建 | [首次执行记录](./runtime/run-initial.log) · [首次构建日志](./runtime/build-initial.log) · [成功构建日志](./runtime/build.log) · [副本源码差异](./runtime/fixture.diff) |
| QEMU/GDB 断点返回 | [运行记录](./runtime/run.log) · [内核反汇编节选](./runtime/disassembly.txt) · [GDB 日志](./runtime/gdb.log) · [串口日志](./runtime/qemu-serial.log) · [初版 SPIE 检查日志](./runtime/gdb-initial-spie.log) |

## 单独汇编

原始验证在 Ubuntu 的 `~/os-lab/lab3-module1-wYZtxs` 中执行。该目录包含 `code/kern/trap/trapentry.S` 和 `code/libs/riscv.h` 的当时副本。若在新的目录重跑，将这两个源文件按上述相对路径复制进去，将 [verify.sh](./assembly/verify.sh) 放在 `code/` 的同级目录，执行以下命令。脚本加载已有工具链，生成 `trapentry.o` 并保留命令和反汇编输出。

```bash
bash -x ./verify.sh > verification.log 2>&1
```

## 运行验证

原始运行目录为 Ubuntu 上的 `~/os-lab/lab3-module1-runtime-hY6obq`。以下命令从留存的 `baseline/` 建立一次新的测试副本，保留原始日志。若原始目录不可用，可先把待测仓库的 `code/` 复制为 `baseline/`，并把 [prepare.py](./runtime/prepare.py)、[run.sh](./runtime/run.sh) 和 [probe.gdb](./runtime/probe.gdb) 放在同级目录。`prepare.py` 应在尚未注入测试的副本上运行。

```bash
(
    set -e
    source ~/compiler-lab/env.sh
    saved="$HOME/os-lab/lab3-module1-runtime-hY6obq"
    replay=$(mktemp -d "$HOME/os-lab/lab3-module1-replay-XXXXXX")
    cp -a "$saved/baseline" "$replay/baseline"
    cp -a "$replay/baseline" "$replay/code"
    cp "$saved/prepare.py" "$saved/run.sh" "$saved/probe.gdb" "$replay/"
    cd "$replay"
    printf 'Replay directory: %s\n' "$PWD"
    python3 prepare.py
    bash -x ./run.sh > run.log 2>&1
    grep -F 'GDB_MODULE1_PASS traps=1' gdb.log
    grep -F 'LAB3_MODULE1_PROBE_PASS traps=1 result=0x3579c' qemu-serial.log
)
```

`run.sh` 使用 `make -B -C code V= DEFS=-Dsbadaddr=stval` 构建，通过已有 QEMU 4.1.1 启动测试内核，并在回环地址的 `12347` 端口连接 GDB。`probe.gdb` 在 `trap()` 的第一条指令读取参数和栈指针，在 `sret` 前后逐步检查恢复地址和状态。串口与 GDB 的两个成功标记共同确认此处的单次断点返回。
