set -eo pipefail
cd -- "$(dirname -- "$0")"
source "$HOME/compiler-lab/env.sh"
set -u
command -v riscv64-unknown-elf-gcc riscv64-unknown-elf-objdump
riscv64-unknown-elf-gcc --version
riscv64-unknown-elf-objdump --version
cd code
sha256sum kern/trap/trapentry.S libs/riscv.h
riscv64-unknown-elf-gcc -Ikern/trap/ \
    -mcmodel=medany -std=gnu99 -Wno-unused -Werror \
    -fno-builtin -Wall -O2 -nostdinc \
    -fno-stack-protector -ffunction-sections -fdata-sections -g \
    -Ilibs/ -Ikern/debug/ -Ikern/driver/ -Ikern/trap/ -Ikern/mm/ -Ikern/arch/ \
    -Dsbadaddr=stval -c kern/trap/trapentry.S -o ../trapentry.o
riscv64-unknown-elf-objdump -dr ../trapentry.o | tee ../trapentry.objdump.txt
