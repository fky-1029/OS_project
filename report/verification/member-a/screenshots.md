# 成员 A 截图状态与回看命令

首次构建运行和原始专项测试的两张真实截图已保存并插入报告。图片保持原样，内容为实际日志回看，没有重新运行实验。

前两项已完成，256 MiB 和异常场景截图均为可选，不作为成员 A 个人模块提交的缺项。以下命令保留用于回看原始日志，不需要因此增加测试。

## 1. 首次构建与 First-Fit 运行（已完成）

文件：[member-a-lab2-first-run.png](../../images/member-a-lab2-first-run.png)。

截图已显示 QEMU 4.1.1、构建输出、default_pmm_manager、31929 个空闲页和检查成功。对应回看命令：

```bash
"$HOME/os-lab/tools/qemu-4.1.1/bin/qemu-system-riscv64" --version
a_first="$HOME/os-lab/lab2-member-a/verification/first-run"
cat "$a_first/build.log"
grep -E 'memory management:|Physical memory detected:|Manageable physical memory end:|Page descriptors:|Initial free pages:|check_alloc_page' "$a_first/run.log"
```

已插入报告 5.1 节首次构建与运行结果之后，图注为“首次构建与运行日志回看”。

## 2. 原始专项测试实际通过（已完成）

文件：[member-a-lab2-page-init-test.png](../../images/member-a-lab2-page-init-test.png)。

截图已显示原始 `[Test 1] Page Init: PASS`、First-Fit 检查成功，以及专项镜像的 31928 个空闲页。对应回看命令：

```bash
a_run="$HOME/os-lab/lab2-member-a/verification/boundary-suite/runs/20261018-204456-543031"
grep -E 'memory management:|Physical memory detected:|Initial free pages:|\[Driver\]|\[Test 1\]|check_alloc_page' "$a_run/default-128MiB.log"
```

已插入报告 5.2 节原始专项 PASS 输出之后，图注为“模块一专项测试日志回看”。

## 3. 256 MiB 受管理上界限制（可选，未补）

建议文件名：`report/images/member-a-lab2-256m-cap.png`。

应看到真实 DTB 大小 `0x10000000`、探测末端 `0x90000000`、管理末端 `0x88000000`，以及布局与 First-Fit 检查结果。命令：

```bash
a_run="$HOME/os-lab/lab2-member-a/verification/boundary-suite/runs/20261018-204456-543031"
grep -E 'Size:|memory management:|Physical memory detected:|Manageable physical memory end:|\[Driver\]|check_alloc_page' "$a_run/native-256MiB.log"
```

图片补好后放到报告 5.2 节场景结果表之后。

## 4. 无空闲页或预期 panic（可选，未补）

可分别保存为 `member-a-lab2-no-free.png`、`member-a-lab2-expected-panic.png`。应明确说明是接口注入，不是缩小了 QEMU 的真实 RAM。

```bash
a_run="$HOME/os-lab/lab2-member-a/verification/boundary-suite/runs/20261018-204456-543031"
grep -E '\[Driver\]|Initial free pages:' "$a_run/no-free-partial-tail.log"
sed -n '/memory management:/,$p' "$a_run/descriptor-array-does-not-fit.log"
```

如果截图环境已不再保留原 Ubuntu 路径，可以在 Ubuntu 仓库根目录改用包内日志路径：首次日志位于 `report/verification/member-a/first-run/`，专项日志位于 `report/verification/member-a/boundaries/logs/`。不能把专项副本的数字标成正式首次运行数字。

## 现场演示时的复现入口（本轮未执行）

在 Ubuntu 仓库根目录执行：

```bash
bash report/verification/member-a/reproduce.sh first
bash report/verification/member-a/reproduce.sh boundaries
```

前者会显示新一轮构建和启动输出；后者会输出新目录与逐场景结果，原始 QEMU 输出在新目录日志中。新截图应标注实际对应的运行，不混用旧日志数字。脚本生成的完整测试副本保留在 Ubuntu 独立目录，不应再复制进交付材料。
