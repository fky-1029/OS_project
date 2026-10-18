#!/usr/bin/env bash
# Run from a Linux checkout; keep all generated copies and outputs outside it.
set -eo pipefail
task_bundle="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
task_repo="$(cd "$task_bundle/../../.." && pwd)"
task_env="${LAB2_ENV:-$HOME/compiler-lab/env.sh}"
task_qemu="${LAB2_QEMU:-$HOME/os-lab/tools/qemu-4.1.1/bin/qemu-system-riscv64}"
source "$task_env"
mkdir -p "$HOME/os-lab"
task_stage="$(mktemp -d "$HOME/os-lab/lab2-a-repro-XXXXXXXX")"
echo "Verification directory: $task_stage"

case "${1:-boundaries}" in
    boundaries)
        for task_name in run_suite.py validation_hooks.h validation_driver.inc; do
            cp "$task_bundle/boundaries/$task_name" "$task_stage/$task_name"
        done
        python3 "$task_stage/run_suite.py" \
            --source "$task_repo/code" --suite "$task_stage" --qemu "$task_qemu"
        ;;
    first)
        cp -a "$task_repo/code" "$task_stage/code"
        exec > >(tee "$task_stage/session.log") 2>&1
        cd "$task_stage/code"
        task_filter='listf_cc=$(filter-out kern/mm/best_fit_pmm.c,$(call listf,$(1),$(CTYPE)))'
        set -x
        "$task_qemu" --version
        riscv64-unknown-elf-gcc --version
        make -B "$task_filter" 2>&1 | tee "$task_stage/build.log"
        set +e
        timeout --signal=TERM --kill-after=2s 10s make qemu \
            QEMU="$task_qemu" "$task_filter" </dev/null 2>&1 | tee "$task_stage/run.log"
        task_status=${PIPESTATUS[0]}
        set -e
        echo "QEMU_EXIT=$task_status"
        if [ "$task_status" -ne 0 ] && [ "$task_status" -ne 124 ]; then exit 1; fi
        grep -F 'memory management: default_pmm_manager' "$task_stage/run.log"
        grep -F 'Physical memory detected:' "$task_stage/run.log"
        grep -F 'check_alloc_page() succeeded!' "$task_stage/run.log"
        if grep -Ein 'panic|assertion.*fail|assert.*fail|unhandled|illegal instruction|segmentation fault|undefined reference|error:' "$task_stage/run.log"; then
            exit 1
        fi
        echo FIRST_FIT_STARTUP_PASS
        ;;
    *)
        echo 'Usage: bash reproduce.sh [first|boundaries]' >&2
        exit 2
        ;;
esac
