# Module A verification results

All dynamic cases run the actual page_init under QEMU 4.1.1.
The original default test runs after page_init and before First-Fit checks.
Expected rejections inspect sentinels, then enter the original non-returning __panic.
Sentinels observe the candidate descriptor region and monitored boundaries; they do not prove absence of all possible memory writes.
No-free cases skip allocation-success checks in the test copy only.

| Scenario | Method | Expected | Actual | Log |
|---|---|---|---|---|
| default-128MiB | QEMU 4.1.1 / real DTB | Original test_page_init PASS; First-Fit PASS | PASS (markers and assertions observed) | [default-128MiB.log](default-128MiB.log) |
| native-64MiB | QEMU 4.1.1 / real DTB | Actual DTB range, all descriptors bounded; First-Fit PASS | PASS (markers and assertions observed) | [native-64MiB.log](native-64MiB.log) |
| native-256MiB | QEMU 4.1.1 / real DTB | Cap at PHYSICAL_MEMORY_END; First-Fit PASS | PASS (markers and assertions observed) | [native-256MiB.log](native-256MiB.log) |
| zero-base | QEMU 4.1.1 / injected DTB-interface values | Panic before descriptor writes or registration | PASS (markers and assertions observed) | [zero-base.log](zero-base.log) |
| zero-size | QEMU 4.1.1 / injected DTB-interface values | Panic before descriptor writes or registration | PASS (markers and assertions observed) | [zero-size.log](zero-size.log) |
| range-overflow | QEMU 4.1.1 / injected DTB-interface values | Panic on memory-range addition overflow | PASS (markers and assertions observed) | [range-overflow.log](range-overflow.log) |
| incompatible-base | QEMU 4.1.1 / injected DTB-interface values | Panic on fixed-mapping mismatch | PASS (markers and assertions observed) | [incompatible-base.log](incompatible-base.log) |
| cannot-cover-kernel-start | QEMU 4.1.1 / injected DTB-interface values | Panic: range cannot cover kernel layout | PASS (markers and assertions observed) | [cannot-cover-kernel-start.log](cannot-cover-kernel-start.log) |
| cannot-cover-kernel-tail | QEMU 4.1.1 / injected DTB-interface values | Panic: kernel tail exceeds usable limit | PASS (markers and assertions observed) | [cannot-cover-kernel-tail.log](cannot-cover-kernel-tail.log) |
| descriptor-array-does-not-fit | QEMU 4.1.1 / injected DTB-interface values | Panic before any descriptor write | PASS (markers and assertions observed) | [descriptor-array-does-not-fit.log](descriptor-array-does-not-fit.log) |
| no-free-partial-tail | QEMU 4.1.1 / injected DTB-interface values | Return safely, init_memmap never called, no First-Fit check | PASS (markers and assertions observed) | [no-free-partial-tail.log](no-free-partial-tail.log) |
| no-free-exact-boundary | QEMU 4.1.1 / injected DTB-interface values | Return safely, init_memmap never called, no First-Fit check | PASS (markers and assertions observed) | [no-free-exact-boundary.log](no-free-exact-boundary.log) |
| unaligned-64MiB-plus-123 | QEMU 4.1.1 / injected DTB-interface values | Injected unaligned end, no partial page registered; First-Fit PASS | PASS (markers and assertions observed) | [unaligned-64MiB-plus-123.log](unaligned-64MiB-plus-123.log) |

Official source unchanged: True
page_init function SHA-256: 60774b955f5899591037932d2ef83d2a59f300c7a3d161e1a2622fecf93f8eff

Not exercised: Best-Fit, Buddy, full grade, malformed raw DTB parsing, hardware outside this QEMU mapping.
