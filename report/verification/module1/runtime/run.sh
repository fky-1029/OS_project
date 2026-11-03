#!/usr/bin/env bash
set -eo pipefail
cd -- "$(dirname -- "$0")"
source "$HOME/compiler-lab/env.sh"
set -u
qemu="$HOME/os-lab/tools/qemu-4.1.1/bin/qemu-system-riscv64"
riscv64-unknown-elf-gcc --version
riscv64-unknown-elf-gdb --version
"$qemu" --version
sha256sum baseline/kern/trap/trapentry.S code/kern/trap/trapentry.S
cmp baseline/kern/trap/trapentry.S code/kern/trap/trapentry.S
make -B -C code V= DEFS=-Dsbadaddr=stval > build.log 2>&1
riscv64-unknown-elf-objdump -d code/bin/kernel | awk '
    / <__alltraps>:$| <lab3_module1_probe>:$| <trap>:$/ { show=1 }
    show { print }
    /^$/ { show=0 }
    / <__trapret>:$| <lab3_module1_breakpoint>:$| <lab3_module1_resume>:$/ {
        if (!show) print
        show=1
    }
' > disassembly.txt
sret=$(awk '/ <__trapret>:$/{seen=1;next} seen && $NF=="sret"{sub(/:$/,"",$1); print $1; exit}' disassembly.txt)
[[ "$sret" =~ ^[[:xdigit:]]+$ ]]
if ss -Hltn 'sport = :12347' | grep -q .; then
    printf 'GDB port 12347 already in use\n' >&2
    exit 1
fi
"$qemu" -machine virt -smp 1 -nographic -monitor none -bios default \
    -device loader,file=code/bin/ucore.img,addr=0x80200000 \
    -serial file:qemu-serial.log -gdb tcp:127.0.0.1:12347 -S \
    > qemu-stderr.log 2>&1 &
qemu_pid=$!
stop_qemu() {
    if kill -0 "$qemu_pid" 2>/dev/null; then
        kill "$qemu_pid"
    fi
    wait "$qemu_pid" || true
}
trap stop_qemu EXIT
for attempt in {1..50}; do
    kill -0 "$qemu_pid"
    if ss -Hltn 'sport = :12347' | grep -q .; then
        break
    fi
    sleep 0.1
done
timeout 45s riscv64-unknown-elf-gdb -nx -batch code/bin/kernel \
    -ex 'set architecture riscv:rv64' -ex "set \$sret_pc = 0x$sret" \
    -x probe.gdb > gdb.log 2>&1
grep -F 'GDB_MODULE1_PASS traps=1' gdb.log
grep -F 'LAB3_MODULE1_PROBE_PASS traps=1 result=0x3579c' qemu-serial.log
if grep -Ei 'kernel panic|assertion failed|ASSERT_FAIL' qemu-serial.log gdb.log; then
    exit 1
fi
