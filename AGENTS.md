# Repository Instructions

## Commit Workflow

- Treat every code change as a self-contained commit candidate.
- For each commit, run the full loop before committing:
  1. Implement the scoped change.
  2. Run `git diff --check` and `kernel/build-kernel-efi`.
  3. Do an independent review of the diff without assuming it is correct.
  4. Fix only real bugs found by review or testing.
  5. Re-review the changed paths after every fix until the review is clean.
  6. Run the relevant PXE/hardware scenario on the machine.
  7. Restore the default EFI build after any temporary test build.
  8. Confirm `git status -sb` contains only expected changes.
  9. Commit and push immediately, unless the user explicitly says not to push.
- Do not batch risk into a later regression pass. A final regression pass may add confidence, but it does not replace per-commit review and hardware validation.
- Do not make empty commits. If a final review is clean, report that no commit was needed.

## Review Requirements

- Review the actual diff and the affected call paths, not just the intended design.
- State the concrete failure modes being checked: wrong state owner, stale global access, missed EOI, lost wakeup, bad timeout path, wrong AP target, stale display source, or broken fault attribution.
- If a bug is fixed, repeat a narrower review of the new control flow and data ownership before testing again.
- Preserve unrelated user changes. Do not revert files or hunks outside the scoped change.

## Required Validation

- Always run:
  - `git diff --check`
  - `kernel/build-kernel-efi`
- Always run real PXE/hardware validation for code changes that affect kernel behavior.
- For AP request, AP context, routing, scheduler, broadcast, IPI, LAPIC, timer, queue, or service changes, run the relevant PXE scenarios:
  - default scheduler PXE
  - explicit target AP1/AP2 PXE when routing or target selection is affected
  - AP UD2 smoke when fault handling, service dispatch, or completion paths are affected
  - HANG short-timeout when timeout, wait, idle, or stopped paths are affected
- For BSP fault display or BSP CPU/IDT paths, run BSP UD2 smoke.
- After any temporary compile-time override, rebuild the default EFI with `kernel/build-kernel-efi` before committing.
- For broad AP request, routing, scheduler, broadcast, fault, timeout, or BAD_OP regression coverage, prefer `pxenode1/run-ap-smoke-suite` over manually chaining individual PXE scripts. It runs the default scheduler, explicit AP1/AP2 targets, AP UD2, HANG short-timeout, BSP UD2, unknown-third, and unknown-seventh scenarios, then restores the default EFI.

## Test Scripts And Visual Review

- Prefer repo scripts for repeatable test actions: build variants, PXE power cycle, wait, screenshot capture, artifact naming, and default EFI restoration.
- Do not treat a script that only captures a screenshot as a pass/fail oracle. The agent must still open the screenshot and visually verify the scenario-specific status lines.
- When running `pxenode1/run-ap-smoke-suite`, inspect every screenshot under the reported `SUITE_OUTPUT_DIR`:
  - default scheduler: `AP SCHED: RR`, broadcast PING OK, stream `HANDLED 8/8`, completion/kick `FAIL=0`.
  - explicit AP1/AP2 targets: `AP SCHED: EXPLICIT`, target APIC matches the scenario, broadcast OK, stream `HANDLED 8/8`, `FAIL=0`.
  - AP UD2: AP fault is vector 6 invalid opcode, request identity/history are present, completion is `TX=1 FAIL=0`, and the stream stops on FAULT.
  - HANG short-timeout: the screen returns, history contains `TIMEOUT`, completion does not falsely increase, and no later refill/publish/kick appears.
  - BSP UD2: `BSP FAULT`, `CURRENT REQUEST: N/A`, `CPU: 0`, `TSS: READY`, and TR/loaded TR look sane.
  - unknown-third and unknown-seventh: history shows BAD_OP at the intended request, later slots are skipped or absent, and no further refill/publish/kick occurs.
- A repo-local skill may be used as the visual review checklist for PXE screenshots. In that model, the skill should first direct the agent to run the relevant script, then inspect the produced screenshots and summarize the observed lines.
- Keep executable test logic in scripts where possible. Use skills for orchestration guidance and visual interpretation, not as a replacement for runnable tests with exit codes.
- If a script is added for a PXE scenario, update this workflow or the repo-local skill so future agents know when to run it and what screen lines to verify.

## Reporting

- Report the exact checks and PXE scenarios run.
- Include screenshot paths for PXE runs and summarize the key on-screen status lines.
- After committing, report:
  - commit hash
  - push result
  - `git status -sb`
  - `git log -1 --oneline`
