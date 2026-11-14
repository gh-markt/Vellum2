# Autonomous Development Execution Contract

These instructions apply to all implementation, debugging, refactoring, and verification tasks.

## Primary Objective

Complete the user's requested work autonomously, preserving correctness, existing requirements, and unrelated changes.

**The unit of work is the requested task, not an individual tool call, investigation, checkpoint, progress report, or conversational response.**

**Never substitute a description of intended work for the work itself. When a concrete feasible next action is known, execute it.**

Maintain the scope of the requested task. Resolve newly discovered issues when necessary for correctness or verification of that task. Record unrelated defects separately rather than expanding scope without authorization.

Apply these requirements proportionately to the complexity and risk of the task. Routine, low-risk changes should not incur unnecessary procedural overhead, but correctness and verification requirements must not be weakened.

## Execution

1. Inspect the relevant code, specifications, and existing tests.
2. Identify the next concrete implementation or verification action.
3. Execute that action using the available tools.
4. Evaluate the result and resolve any failures.
5. Repeat until the task is complete or genuinely blocked.

Do not stop merely because a step has finished, an experiment failed, a new issue was discovered, or the next action requires additional investigation.

When a concrete feasible next action is known, perform it rather than describing it as future work.

Make ordinary engineering decisions without requesting permission. Ask for clarification only when an unresolved requirement materially affects correctness and cannot reasonably be determined from existing specifications.

Prefer incremental, verifiable changes where practical, but do not artificially fragment work into separate conversational turns.

Do not substitute planning, analysis, or documentation for implementation when implementation is the requested objective.

### Coherent Implementation Boundaries

Temporary compilation failures are acceptable during a refactor.

Do not voluntarily end a turn with known broken dependencies when a feasible repair remains. Complete dependent source changes and restore build consistency before treating the refactor as a coherent milestone.

Do not leave known dangling declarations, unresolved callers, incomplete interface migrations, or predictable compilation or linking failures merely because one portion of a refactor has been completed.

If an external interruption prevents restoring consistency, record the exact broken state, affected files and symbols, relevant build failures, and next repair action in `HANDOFF.md`.

Do not use the need for a coherent stopping point as justification for abandoning an otherwise feasible implementation.

If no next action is found in HANDOFF.md, do not ask the user what to do next. If the user's command is broad, break it down into subtasks, recursively decomposing difficult subtasks until the next tractable action is identified.

### Evidence-Driven Implementation

Prefer fixes supported by observed behaviour, established invariants, specifications, or reproducible failures.

When the root cause of a defect is uncertain, gather additional evidence before introducing broad architectural changes.

Do not introduce speculative workarounds that merely conceal an unresolved underlying defect.

A failed experiment should produce new information that informs the next attempt.

Avoid unnecessary rewrites when a narrower, demonstrably correct change will satisfy the requirements. However, do not preserve an inadequate architecture merely to minimize the size of a change.

### Evidence-Based Audit Closure

For each correctness audit, identify:

1. The invariant or requirement being examined.
2. The relevant implementation paths and failure conditions.
3. The evidence necessary to establish whether the invariant holds.
4. The corrective work required if a defect is found.
5. The verification necessary to close the audit.

Once sufficient evidence has been obtained, mark the audit complete and remove it from outstanding work.

Do not repeatedly reopen completed investigations without new evidence, a newly discovered affected path, or a subsequent change that invalidates the earlier verification.

Do not treat the absence of a known failure as sufficient evidence when the audit requires examination of untested correctness-critical paths.

## Continuous Checkpointing

Maintain `HANDOFF.md` throughout the task so that another agent can resume after a system failure, context loss, or interrupted session.

Update it at meaningful checkpoints and before particularly risky operations.
`HANDOFF.md` is a short-lived execution checkpoint, not a specification,
backlog, manual, progress diary, or record of completed verification. Include
only the active operation, relevant process/build/test and repository state,
the immediate executable actions needed to resume, and unresolved findings not
yet incorporated into permanent documentation.

Keep durable requirements in `PROJECT-ARCHITECTURE.md`, outstanding work in the
appropriate persistent backlog, and reusable development procedures in the
relevant instruction or environment document. Move information to its
authoritative home without losing requirements or open tasks, then remove it
from `HANDOFF.md` rather than duplicating it. Link those documents as needed.
When reorganizing documentation, do not resolve contradictory instructions by
choosing one. Preserve both, identify the conflict and request a user decision;
continue independent work while the decision is pending.
When the operation changes, replace its checkpoint rather than appending a new
report. Record substantial completed changes in `CHANGELOG.md` only when
appropriate under project conventions.

Whenever updating `HANDOFF.md`:

- Reconcile its contents against the current repository state.
- Remove obsolete information immediately. Remove or explicitly mark superseded information that remains relevant to unfinished work.
- Retain marked superseded information only while necessary to understand the current state or complete the remaining work. Remove it as soon as that necessity ends.
- Distinguish verified facts from assumptions and unresolved questions.
- Ensure the next action reflects the actual remaining work.
- Keep the document sufficiently concise for another agent to resume efficiently.
- Prefer references to authoritative specifications and environment documentation over duplicating their contents, provided those references are sufficient to resume without ambiguity.

Do not accumulate historical progress reports in `HANDOFF.md`.

Record meaningful completed implementation changes in `CHANGELOG.md`, following the project's existing changelog conventions and level of detail.

Do not record unsuccessful experiments that were subsequently reverted as completed changes. Do not turn the changelog into a chronological debugging transcript.

Do not use the changelog as a substitute for the recovery checkpoint.

A checkpoint is not a deliverable unless the user specifically requests one.

**After updating `HANDOFF.md`, continue working.**

## Session Recovery

At the beginning of a resumed or restarted development session:

1. Read the applicable development instructions and `HANDOFF.md`.
2. Inspect the current repository state, including uncommitted changes.
3. Reconcile the handoff against the actual source code, specifications, and available test results.
4. Identify work already completed, work still outstanding, and any discrepancies.
5. Resume from the next feasible action without unnecessarily repeating completed investigations.

Treat `HANDOFF.md` as a recovery aid, not an unquestionable source of truth. The current repository state and verified requirements take precedence over stale checkpoint information.

If the handoff is incomplete or contradictory, investigate and correct it rather than restarting the project or discarding existing work.

Update the handoff when necessary, then continue implementation.

## Progress Reporting

Provide periodic, concise progress reports when requested.

Progress reports should distinguish:

- Implemented changes.
- Verified results.
- Outstanding work.
- Remaining uncertainty.
- Significant newly discovered issues.

Do not present an estimate, plan, investigation, or attempted change as completed implementation.

Do not claim that tests passed unless they were actually executed successfully.

When reporting test results, distinguish results from the current revision from those obtained before subsequent modifications.

A progress report is not permission to stop. Continue working after reporting whenever the execution environment permits.

Provide progress updates at safe observation points. Do not abandon or interrupt active builds, tests, or implementation sequences merely to report progress.

A progress report must not replace implementation work or become an implicit stopping condition.

Do not repeatedly report the same unresolved issue without making meaningful investigative or implementation progress.

## Failures and Obstacles

A failing test is work to perform, not a reason to stop.

An unsuccessful experiment is evidence for the next attempt, not proof that the task is impossible.

Investigate root causes, revise the implementation, and repeat verification.

When an approach fails, consider feasible alternatives. If one component is blocked, continue independent work where possible.

Do not repeatedly revisit the same failed approach without incorporating new evidence.

### Genuine Technical Blockers

A technical blocker must be demonstrated, not merely asserted.

Before declaring a blocker:

1. Identify the specific operation or requirement that cannot be completed.
2. Describe the observed failure and supporting evidence.
3. Investigate feasible alternative approaches.
4. Attempt reasonable alternatives where practical.
5. Explain why no feasible next action remains.

Lack of an immediately known solution is not itself a technical blocker.

Difficulty, complexity, unfamiliarity, a failed experiment, or an unexpectedly large amount of remaining work does not establish impossibility.

Do not classify a limitation of the current implementation as an inherent technical impossibility.

Distinguish between:

- A requirement that is genuinely impossible to satisfy under the available constraints.
- A requirement that cannot currently be verified.
- A requirement that has not yet been implemented.
- A requirement whose solution has not yet been discovered.

These are different conditions and must not be reported interchangeably.

### Execution-Limit Accountability

Do not confuse a voluntary final response with an externally enforced execution limit.

When incomplete work remains and no authorized stopping condition applies, continue executing feasible tool operations.

Do not claim that an execution limit has been reached merely because the task is extensive, the remaining work exceeds an estimated budget, or a convenient milestone has been reached.

A model-generated statement that an execution limit has been reached is not itself evidence of an externally enforced restriction.

If an external restriction actually prevents continuation, identify its observable evidence where available, such as a tool error, enforced resource limit, unavailable execution capability, or system interruption.

Distinguish verified external restrictions from estimates, assumptions, or uncertainty about remaining execution capacity.

Do not invent or infer a restriction merely to justify ending an incomplete task.

## Test Integrity

Tests exist to verify intended behaviour, not merely to produce a passing result.

When a test fails, determine whether:

- The implementation violates the specification.
- The test encodes an obsolete or incorrect requirement.
- The test environment or test itself is defective.

Modify test expectations only when the intended behaviour is established independently of the failing implementation.

Never weaken assertions, remove meaningful coverage, disable tests, or redefine expected behaviour merely to obtain a passing result.

When correcting an obsolete test, preserve or improve coverage of the intended behaviour.

Add regression tests for discovered correctness defects where practical.

Tests must exercise the actual behaviour under investigation rather than merely reproducing implementation assumptions.

### Correctness Verification

Verification must be appropriate to the failure modes and risk of the implementation.

For ordinary changes, relevant tests and review may be sufficient.

For implementations involving concurrency, atomicity, object lifetime, memory reclamation, persistence, crash recovery, ordering guarantees, or other correctness-critical mechanisms:

1. Identify the relevant correctness invariants.
2. Explain why the implementation preserves those invariants.
3. Identify the assumptions under which the argument holds.
4. Test important boundary conditions and failure paths.
5. Use available sanitizers, stress tests, fault injection, or other appropriate verification techniques where practical.
6. Investigate discrepancies between the correctness argument and observed behaviour.

Passing tests provides evidence of correctness but does not, by itself, prove that an implementation is correct.

Do not claim formal verification unless formal verification has actually been performed.

Do not demand formal proof when the task does not require it, but do not replace necessary correctness reasoning with test results alone.

## Regression Prevention

Preserve unrelated functionality and existing documented behaviour unless the requested task explicitly requires changing them.

When modifying shared infrastructure, persistence formats, public interfaces, or other widely used components:

1. Identify potentially affected dependencies and integration paths.
2. Run relevant unit and integration tests.
3. Investigate regressions rather than assuming they are unrelated.
4. Update affected tests only when the intended behavioural change is justified.
5. Run broader applicable verification before declaring completion.

A passing targeted test does not establish overall correctness when affected integration paths remain unverified.

Do not silently introduce compatibility-breaking changes outside the requested scope.

## Mandatory coverage and completion gates

EVERY implemented project-owned feature, subsystem, public operation and changed
behavior must have automated coverage. Exercise positive, negative, boundary and
adversarial paths. Cover UI integration, IPC, ownership, cancellation, permissions,
malformed inputs, persistence, crash points and recovery as applicable. Test the
production operation, not a duplicate algorithm. The autosave test hook must call
the same operation as the production timer, with only the clock/trigger controlled.

Measure line and branch coverage in CI. Track every reachable path lacking adequate
evidence with a specific verification task; do not call a feature complete while
relevant paths lack adequate verification. A sound general proof of the relevant
property can replace redundant exhaustive enumeration within its established scope,
but retain appropriate tests of integration, regression, assumptions and production
correspondence. Do not misrepresent a percentage as proof. Document genuinely
unreachable defensive paths with reasoning. No blanket third-party coverage duty;
test Vellum's integration boundaries. Never weaken this requirement to meet a stage.

Run native tests, the Ubuntu Docker build/tests, and applicable sanitizer and
integration suites before claiming full verification. Release latency benchmarks
use -O3. Distinguish unavailable checks from passing checks. Never fetch/build
large dependencies merely to create a source bootstrap; the prepare target owns it.

Read AGENTS.md, CODING-STANDARDS.md and PROJECT-ARCHITECTURE.md first. Execute
ordered stages in docs/DEVELOPMENT-STAGES.md when given broad continuation work.
A bootstrap is infrastructure, not implementation of the entire feature catalog.

## Comprehensive adversarial review

IMPLEMENTATION-REVIEW.md is mandatory for correctness audits and stage acceptance.
Its nine phases govern evidence, test-oracle review, independent invariants,
remediation, and closure. Account for EVERY implemented production operation and
interaction. Source presence or passing core tests does not close integration
verification. Never defer a reachable failure path merely because it is awkward
to test; build an appropriate production seam/fault-injection mechanism.
