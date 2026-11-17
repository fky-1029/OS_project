# Lab4 模块一验证记录

负责人为 2413671-吕旭婧。开发与验证基于 `lab4` 的 `e314eca5641bff8fbefe92c78fa615fb544223bc`。QEMU 测试副本的 `pmm.c` 和正式实现的 Git 内容哈希均为 `653a57befefdfaf7b3b1582462aca9e01d7cfb46`。 [模块一实现](../../../code/kern/mm/pmm.c) · [实验报告](../../report.md)

## 原始材料

| 材料 | 内容 |
|---|---|
| [lab4-module1-build.log](lab4-module1-build.log) | `pmm.o` 首次编译失败和修正后的成功记录，含实际 GCC 命令、RV64 ELF 格式和符号检查。 |
| [01-original-build.log](01-original-build.log) | 测试副本首次完整构建停在 `kmalloc.c:92` 的 GCC 15 整数到指针诊断。 |
| [02-original-build.log](02-original-build.log) · [串口日志](02-original-qemu.log) · [退出码](02-original-qemu.exit) | 带 `-Wno-error=int-conversion` 的构建，执行原有自检，两项均通过，QEMU 退出码为 `0`。 |
| [03-extra-build.log](03-extra-build.log) · [串口日志](03-extra-qemu.log) · [退出码](03-extra-qemu.exit) | 加入额外测试后的构建和运行，原有自检再次通过，额外断言全部执行完毕，QEMU 退出码为 `0`。 |
| [test-init.c](test-init.c) · [module1-test.c](module1-test.c) | 实际运行过的测试副本入口与额外测试。入口保留设备树解析和原样的 `pmm_init()`，完成验证后调用 SBI 关机。额外测试临时替换分配器回调以控制失败时机，测试调用结束后恢复回调。 |

## 单独编译

在 Ubuntu 的独立开发目录运行以下命令。归档日志中的首次失败来自实现初版使用未定义的 `true` 和 `false`，当前实现采用工程已有的 `1` 和 `0`。

```bash
cd "$HOME/os-lab/lab4-module1/code"
. "$HOME/compiler-lab/env.sh"
make V= obj/kern/mm/pmm.o
```

## 使用独立副本复现 QEMU 测试

从 Windows 开发目录把本次验证所需的三个源文件传到 Ubuntu 的暂存目录。

```powershell
Set-Location -LiteralPath 'D:\lvxujing\Download\CodexHomework\操作系统\lab4\OS_project-lab4-module1'
ssh ubuntu-vm 'mkdir -p "$HOME/os-lab/lab4-module1-replay-input"'
scp code/kern/mm/pmm.c reports/verification/module1/test-init.c reports/verification/module1/module1-test.c ubuntu-vm:os-lab/lab4-module1-replay-input/
```

在 Ubuntu 用一个新目录复制基线，然后安装暂存的实现和测试入口。命令统一上传文件的行尾为 LF，并核对实现内容哈希。原有运行使用的是 `~/os-lab/lab4-module1-test`，以下复现使用新的 `~/os-lab/lab4-module1-replay`，避免覆盖已有日志与构建产物。

```bash
set -e
source_tree="$HOME/os-lab/lab4-module1"
inputs="$HOME/os-lab/lab4-module1-replay-input"
replay="$HOME/os-lab/lab4-module1-replay"
logs="$HOME/os-lab/lab4-module1-replay-logs"
test ! -e "$replay"
test ! -e "$logs"
git clone --local --single-branch --branch lab4 "$source_tree" "$replay"
test "$(git -C "$replay" rev-parse HEAD)" = e314eca5641bff8fbefe92c78fa615fb544223bc
sed 's/\r$//' "$inputs/pmm.c" > "$replay/code/kern/mm/pmm.c"
test "$(git -C "$replay" hash-object --path=code/kern/mm/pmm.c code/kern/mm/pmm.c)" \
  = 653a57befefdfaf7b3b1582462aca9e01d7cfb46
sed 's/\r$//' "$inputs/test-init.c" > "$replay/code/kern/init/init.c"
mkdir "$logs"
cd "$replay/code"
. "$HOME/compiler-lab/env.sh"
```

在同一个 Ubuntu shell 依次运行两次构建与启动。第一轮只运行原有自检，第二轮加入额外测试。`DEFS=-Wno-error=int-conversion` 只用于测试构建，以保留 `kmalloc.c` 的诊断并完成构建。构建输出、QEMU 串口和退出码分别保存。

```bash
qemu="$HOME/os-lab/tools/qemu-4.1.1/bin/qemu-system-riscv64"

run_qemu() {
    label="$1"
    status=0
    (set -x; timeout 15s "$qemu" -machine virt -nographic -bios default \
      -device loader,file=bin/ucore.img,addr=0x80200000 < /dev/null) \
      > "$logs/$label-qemu.log" 2>&1 || status=$?
    printf '%s\n' "$status" > "$logs/$label-qemu.exit"
    cat "$logs/$label-qemu.log"
    test "$status" -eq 0
    grep -F 'check_pgdir() succeeded!' "$logs/$label-qemu.log"
    grep -F 'check_boot_pgdir() succeeded!' "$logs/$label-qemu.log"
}

(set -x; make V= OBJDIR=obj-module1-test DEFS=-Wno-error=int-conversion bin/ucore.img) \
  > "$logs/02-original-build.log" 2>&1
run_qemu 02-original

sed 's/\r$//' "$inputs/module1-test.c" > kern/init/module1-test.c
(set -x; make V= OBJDIR=obj-module1-test DEFS=-Wno-error=int-conversion bin/ucore.img) \
  > "$logs/03-extra-build.log" 2>&1
run_qemu 03-extra
grep -F 'all extra assertions completed' "$logs/03-extra-qemu.log"
```

额外测试中，同页降权检查了 PTE 上的写权限位和实际读操作。主动触发降权后的写入异常或删除后的访问异常、全组进程与调度测试、完整评分均未运行。截图位置与日志回看命令见报告第五节。 [实验报告](../../report.md)
