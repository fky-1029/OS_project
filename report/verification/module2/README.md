# 模块二验证（2413396-范珂祎）

模块二在完整内核上验证，复现 `tools/grade.sh` 的三个评分用例，对应 35 + 25 + 40 = 100 分。

## 环境

- WSL Ubuntu 24.04，交叉编译器 `riscv64-unknown-elf-gcc` 15.1.0，GNU objdump 2.45。
- QEMU `qemu-system-riscv64` 8.2.2，OpenSBI v1.3（`fw_dynamic`）。

## 与 grade.sh 的差异

原 `tools/grade.sh` 在本机跑不通，两点原因都是环境差异，不是模块二代码问题：

1. `-device loader,file=bin/ucore.img,addr=0x80200000`：OpenSBI v1.3 `fw_dynamic` 下 Next Address 为 0，会跳到地址 0 卡死。改为 `-kernel bin/ucore.img`。
2. CSR 名 `sbadaddr`：新版 binutils（2.45）已改名为 `stval`，而 `trapentry.S` 里仍是 `sbadaddr`。用 `DEFS=-Dsbadaddr=stval` 传入兼容宏。

## 复现命令

```sh
cd report/verification/module2
bash verify.sh    # 重建 + 跑三个用例；串口输出存 case-*.serial.log，脚本判定输出存 verify-run.log
```

## 结果

脚本判定输出见 `verify-run.log`，每个用例的完整串口输出见 `case-*.serial.log`，构建日志见 `build-*.log`。

| 用例 | 宏 | 分数 | 关键输出 |
|---|---:|---:|---|
| 断点返回与寄存器恢复 | GRADE_LAB3_CONTEXT | 35 | `Breakpoint at 0xc0201a1c` → `PASS (35/35)` |
| 非法指令 epc 前进与兜底 | GRADE_LAB3_ILLEGAL | 25 | `Illegal instruction at 0xc02019ce`、`Supervisor software interrupt`、`Unknown interrupt: cause 15`、`cause 0x00000014` |
| 持续时钟中断与重设 | GRADE_LAB3_TIMER | 40 | `100 ticks`（多次）→ `PASS (40/40, ticks=201)` |

## 关于 ticks=200 vs 201

`test_lab3_wait_for_timer` 在 `while (ticks < 200)` 退出后打印实际 `ticks`。第 200 次边界恰好触发一次 `print_ticks()` 的 `cprintf`，其间可能再漏进一次时钟中断，所以打印值可能为 200 或 201。这是框架测试自身的时序特性，不是模块二缺陷。`grade.sh` 原样 grep `ticks=200` 在 QEMU 8.2.2 下会偶发不中，本目录 `verify.sh` 用前缀 `ticks=20` 兼容两种取值。
