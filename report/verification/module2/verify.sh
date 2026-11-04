#!/bin/sh
# Lab3 模块二验证：在 QEMU 8.2.2 / OpenSBI v1.3 下复现 tools/grade.sh 的三个用例，
# 并把每个用例的串口输出与构建日志保存到本目录作为证据。
#
# 与 tools/grade.sh 的差异：
#   1. 用 -kernel $(UCOREIMG) 而非 -device loader（fw_dynamic 下 loader 方式会卡死在地址 0）；
#   2. 加 -Dsbadaddr=stval（新版 binutils 已把 CSR 名 sbadaddr 改名为 stval）。
set -u

SRC=/mnt/d/others/OS_project/code
OUT_DIR=$(cd "$(dirname "$0")" && pwd)
TMP=/tmp/lab3_module2_verify
export PATH=/home/fanky/riscv-elf-toolchains/bin:/usr/local/bin:/usr/bin:/bin

rm -rf "$TMP"
mkdir -p "$TMP"
( cd "$SRC" && tar -c --exclude=obj --exclude=bin --exclude=.git . ) | ( cd "$TMP" && tar -xf - )
python3 "$SRC/tools/inject.py" "$TMP"

run_case() {
    label="$1"; define="$2"; sentinel="$3"; seconds="$4"; shift 4
    serial="$OUT_DIR/case-${label}.serial.log"
    build="$OUT_DIR/build-${label}.log"
    echo "===== $label ($define) ====="
    if ! ( cd "$TMP" && make --quiet --no-print-directory clean >/dev/null 2>&1 ); then
        echo "FAIL(clean)"; return
    fi
    if ! ( cd "$TMP" && make --quiet --no-print-directory "DEFS+=-Dsbadaddr=stval -D${define}" >"$build" 2>&1 ); then
        echo "FAIL(build)"; tail -n 40 "$build"; return
    fi
    timeout "${seconds}s" qemu-system-riscv64 \
        -machine virt -nographic -bios default \
        -kernel "$TMP/bin/ucore.img" \
        -serial file:"$serial" -monitor none -no-reboot >/dev/null 2>&1 || true
    pass=1
    for pat in "$sentinel" "$@"; do
        if grep -F "$pat" "$serial" >/dev/null 2>&1; then
            echo "  OK   [$pat]"
        else
            echo "  MISS [$pat]"
            pass=0
        fi
    done
    if [ "$pass" = 1 ]; then echo "  => PASS"; else echo "  => FAIL"; fi
}

run_case context GRADE_LAB3_CONTEXT \
    "[Test 1] Trap entry and breakpoint return: PASS" 10 \
    "Breakpoint at"

run_case illegal GRADE_LAB3_ILLEGAL \
    "[Test 2] Illegal instruction return: PASS" 10 \
    "Illegal instruction at" \
    "Supervisor software interrupt" \
    "Unknown interrupt: cause 15" \
    "cause    0x00000014"

# 注意：test_lab3_wait_for_timer 在 ticks>=200 后打印实际 ticks。因第 200 次边界恰好触发
# 一次 print_ticks 的 cprintf，其间可能再漏进一次时钟中断，最终值可能为 200 或 201，
# 故这里用前缀 "ticks=20" 匹配两种情况（grade.sh 原样写 "ticks=200" 在 QEMU 8.2.2 下会偶发不中）。
run_case timer GRADE_LAB3_TIMER \
    "[Test 3] Sustained supervisor timer: PASS" 30 \
    "100 ticks" \
    "ticks=20"
