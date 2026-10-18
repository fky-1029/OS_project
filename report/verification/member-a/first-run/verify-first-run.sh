#!/usr/bin/env bash
set -eo pipefail

source "$HOME/compiler-lab/env.sh"
task_root="$HOME/os-lab/lab2-member-a"
task_logs="$task_root/verification/first-run"
task_qemu="$HOME/os-lab/tools/qemu-4.1.1/bin/qemu-system-riscv64"
task_filter='listf_cc=$(filter-out kern/mm/best_fit_pmm.c,$(call listf,$(1),$(CTYPE)))'

exec > >(tee "$task_logs/session.log") 2>&1
cd "$task_root/code"
set -x
date -Is
"$task_qemu" --version
riscv64-unknown-elf-gcc --version
sha256sum kern/mm/pmm.c Makefile kern/mm/default_pmm.c kern/mm/best_fit_pmm.c tools/test_lab2.c | tee "$task_logs/source-sha256.log"

make -B "$task_filter" 2>&1 | tee "$task_logs/build.log"
echo BUILD_PASS
if grep -F '+ cc kern/mm/best_fit_pmm.c' "$task_logs/build.log"; then
    echo 'ERROR: unfinished Best-Fit was not excluded'
    exit 1
fi

set +e
timeout --signal=TERM --kill-after=2s 10s make qemu QEMU="$task_qemu" "$task_filter" </dev/null 2>&1 | tee "$task_logs/run.log"
task_status=${PIPESTATUS[0]}
set -e
echo "QEMU_EXIT=$task_status"
if [ "$task_status" -ne 124 ] && [ "$task_status" -ne 0 ]; then
    echo 'ERROR: unexpected QEMU exit status'
    exit 1
fi

grep -F 'memory management: default_pmm_manager' "$task_logs/run.log"
grep -F 'Physical memory detected:' "$task_logs/run.log"
grep -F 'check_alloc_page() succeeded!' "$task_logs/run.log"
if grep -Ein 'panic|assertion.*fail|assert.*fail|unhandled|illegal instruction|segmentation fault|undefined reference|error:' "$task_logs/run.log"; then
    echo 'ERROR: abnormal output detected'
    exit 1
fi
echo FIRST_FIT_STARTUP_PASS
