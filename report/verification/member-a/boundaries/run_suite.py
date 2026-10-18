#!/usr/bin/env python3
"""Generate an isolated test copy and exercise the real page_init in QEMU.

No cleanup, git operations, source-tree edits, or alternative initializer.
"""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys


def sha(data):
    return hashlib.sha256(data).hexdigest()


def snapshot(root):
    return {str(p.relative_to(root)): sha(p.read_bytes())
            for p in sorted(root.rglob("*")) if p.is_file()}


def page_function(text):
    start = text.index("static void page_init(void) {")
    match = re.search(r"(?m)^}\r?\n(?=\r?\n/\* pmm_init)", text[start:])
    if not match:
        raise RuntimeError("page_init boundary is missing or ambiguous")
    finish = start + match.start() + 1
    return start, finish, text[start:finish]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--suite", type=Path, required=True)
    parser.add_argument("--qemu", type=Path, required=True)
    args = parser.parse_args()
    source = args.source.resolve()
    suite = args.suite.resolve()
    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S-%f")
    run = suite / "runs" / stamp
    run.mkdir(parents=True, exist_ok=False)
    before = snapshot(source)
    code = run / "code"
    shutil.copytree(source, code)
    pmm = code / "kern/mm/pmm.c"
    original = pmm.read_bytes().decode("utf-8")
    _, finish, function = page_function(original)
    # Insert around the original function, leaving its bytes unchanged.
    text = (original[:finish] + '\n#include "validation_driver.inc"\n' +
            original[finish:])
    text = text.replace("//lab2 function1 2413671",
                        '#include "validation_hooks.h"\n//lab2 function1 2413671', 1)
    text = text.replace("    page_init();",
                        "    validation_prepare();\n    page_init();\n"
                        "    validation_check_layout();", 1)
    text = text.replace("    check_alloc_page();",
                        "    if (!validation_no_free) check_alloc_page();", 1)
    anchor = "satp_virtual, satp_physical);"
    if text.count(anchor) != 1:
        raise RuntimeError("pmm_init completion anchor is ambiguous")
    text = text.replace(anchor, anchor + "\n    validation_finish();", 1)
    # The new include separates the original closing brace from pmm_init.
    copied_function = text[text.index("static void page_init(void) {"):]
    copied_function = copied_function.split('\n#include "validation_driver.inc"', 1)[0]
    if copied_function != function:
        raise RuntimeError("page_init function body changed while adding test hooks")
    pmm.write_bytes(text.encode("utf-8"))
    for name in ("validation_hooks.h", "validation_driver.inc"):
        shutil.copyfile(suite / name, code / "kern/mm" / name)
    shutil.copyfile(code / "tools/test_lab2.h", code / "kern/mm/test_lab2.h")
    original_test = code / "tools/test_lab2.c"
    copied_test = code / "kern/mm/validation_original_test.c"
    shutil.copyfile(original_test, copied_test)
    if copied_test.read_bytes() != original_test.read_bytes():
        raise RuntimeError("framework tests changed")
    for rel, digest in before.items():
        if rel != "kern/mm/pmm.c" and sha((code / rel).read_bytes()) != digest:
            raise RuntimeError("non-test source changed: " + rel)

    manifest = {
        "source": str(source), "test_copy": str(code),
        "page_init_sha256": sha(function.encode("utf-8")),
        "test_copy_page_init_sha256": sha(copied_function.encode("utf-8")),
        "original_test_sha256": sha(original_test.read_bytes()),
        "copied_test_sha256": sha(copied_test.read_bytes()),
        "only_existing_copy_file_modified": "kern/mm/pmm.c",
        "official_source_before": before,
    }
    (run / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    print("RUN_DIRECTORY=" + str(run), flush=True)

    commands = []

    def execute(command, log):
        commands.append(shlex.join([str(v) for v in command]))
        (run / "commands.log").write_text("\n".join(commands) + "\n", encoding="utf-8")
        with log.open("wb") as out:
            result = subprocess.run([str(v) for v in command], cwd=code,
                                    stdout=out, stderr=subprocess.STDOUT)
        return result.returncode, log.read_text(encoding="utf-8", errors="replace")

    _, version = execute([args.qemu, "--version"], run / "qemu-version.log")
    if "version 4.1.1" not in version:
        raise RuntimeError("QEMU must be 4.1.1")
    filter_arg = "listf_cc=$(filter-out kern/mm/best_fit_pmm.c,$(call listf,$(1),$(CTYPE)))"
    build_status, build_text = execute(["make", "-B", filter_arg], run / "build.log")
    if build_status or "+ cc kern/mm/best_fit_pmm.c" in build_text:
        raise RuntimeError("isolated test kernel did not build; see build.log")

    scenarios = [
        (0, "default-128MiB", 128, "Original test_page_init PASS; First-Fit PASS"),
        (1, "native-64MiB", 64, "Actual DTB range, all descriptors bounded; First-Fit PASS"),
        (2, "native-256MiB", 256, "Cap at PHYSICAL_MEMORY_END; First-Fit PASS"),
        (10, "zero-base", 128, "Panic before descriptor writes or registration"),
        (11, "zero-size", 128, "Panic before descriptor writes or registration"),
        (12, "range-overflow", 128, "Panic on memory-range addition overflow"),
        (13, "incompatible-base", 128, "Panic on fixed-mapping mismatch"),
        (14, "cannot-cover-kernel-start", 128, "Panic: range cannot cover kernel layout"),
        (15, "cannot-cover-kernel-tail", 128, "Panic: kernel tail exceeds usable limit"),
        (16, "descriptor-array-does-not-fit", 128, "Panic before any descriptor write"),
        (17, "no-free-partial-tail", 128, "Return safely, init_memmap never called, no First-Fit check"),
        (18, "no-free-exact-boundary", 128, "Return safely, init_memmap never called, no First-Fit check"),
        (19, "unaligned-64MiB-plus-123", 128, "Injected unaligned end, no partial page registered; First-Fit PASS"),
    ]
    results = []
    for case, name, ram, expected in scenarios:
        command = ["timeout", "--signal=TERM", "--kill-after=2s", "4s", str(args.qemu),
                   "-machine", "virt", "-m", str(ram) + "M", "-nographic", "-bios", "default",
                   "-device", "loader,file=bin/ucore.img,addr=0x80200000",
                   "-device", "loader,addr=0x81000000,data=" + str(case) + ",data-len=4"]
        log = run / (name + ".log")
        status, output = execute(command, log)
        errors = []
        if status not in (0, 124):
            errors.append("unexpected QEMU status " + str(status))
        if "[Driver] INPUT id=" + str(case) + " " not in output:
            errors.append("scenario did not enter the driver")
        if "memory management: default_pmm_manager" not in output:
            errors.append("First-Fit manager is not active")
        rejected = 10 <= case <= 16
        if rejected:
            required = ["[Driver] REJECTION_CHECK id=" + str(case) + " ",
                        "candidate_and_guards_unchanged=1",
                        "[Driver] EXPECTED_REJECTION id=" + str(case) + " ",
                        "kernel panic at", "page_init:"]
            if "[Driver] SCENARIO_PASS" in output:
                errors.append("invalid initialization returned successfully")
        else:
            required = ["[Driver] LAYOUT_PASS id=" + str(case) + " ",
                        "[Driver] SCENARIO_PASS id=" + str(case)]
            if case == 0:
                required.append("[Test 1] Page Init: PASS")
            if case in (17, 18):
                required.append("[Driver] NO_FREE_PASS id=" + str(case) + " init_memmap_calls=0")
                if "check_alloc_page() succeeded!" in output:
                    errors.append("no-free case incorrectly ran First-Fit allocation checks")
            else:
                required.append("check_alloc_page() succeeded!")
            if "kernel panic at" in output:
                errors.append("unexpected kernel panic")
        for marker in required:
            if marker not in output:
                errors.append("missing " + marker)
        if re.search(r"assertion failed|Page Init: FAIL|Best-Fit:|illegal instruction|unhandled", output, re.I):
            errors.append("assertion failure, forbidden test, or unexpected trap")
        row = {"id": case, "scenario": name,
               "method": "QEMU 4.1.1 / real DTB" if case <= 2 else "QEMU 4.1.1 / injected DTB-interface values",
               "qemu_ram_mib": ram, "expected": expected, "exit_status": status,
               "result": "PASS" if not errors else "FAIL", "errors": errors, "log": log.name,
               "evidence": [line for line in output.splitlines()
                            if line.startswith("[Driver]") or line.startswith("[Test 1]") or
                            "check_alloc_page() succeeded!" in line or "kernel panic at" in line]}
        results.append(row)
        print(name + ": " + row["result"] + ("; " + "; ".join(errors) if errors else ""), flush=True)

    after = snapshot(source)
    manifest["official_source_after"] = after
    manifest["official_source_unchanged"] = before == after
    (run / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    if before != after:
        raise RuntimeError("official source changed during validation")
    (run / "results.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
    lines = ["# Module A verification results", "",
             "All dynamic cases run the actual page_init under QEMU 4.1.1.",
             "The original default test runs after page_init and before First-Fit checks.",
             "Expected rejections inspect sentinels, then enter the original non-returning __panic.",
             "Sentinels observe the candidate descriptor region and monitored boundaries; they do not prove absence of all possible memory writes.",
             "No-free cases skip allocation-success checks in the test copy only.", "",
             "| Scenario | Method | Expected | Actual | Log |", "|---|---|---|---|---|"]
    for row in results:
        actual = row["result"] + (": " + "; ".join(row["errors"]) if row["errors"] else " (markers and assertions observed)")
        lines.append("| " + " | ".join([row["scenario"], row["method"], row["expected"], actual,
                                         "[" + row["log"] + "](" + row["log"] + ")"]) + " |")
    lines += ["", "Official source unchanged: " + str(before == after),
              "page_init function SHA-256: " + manifest["page_init_sha256"],
              "", "Not exercised: Best-Fit, Buddy, full grade, malformed raw DTB parsing, hardware outside this QEMU mapping."]
    (run / "results.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    (suite / "latest-run.txt").write_text(str(run) + "\n", encoding="utf-8")
    print("RESULTS=" + str(run / "results.md"), flush=True)
    return 0 if all(row["result"] == "PASS" for row in results) else 1


if __name__ == "__main__":
    sys.exit(main())
