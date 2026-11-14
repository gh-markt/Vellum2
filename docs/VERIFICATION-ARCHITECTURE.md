# Verification architecture

This document is authoritative under PROJECT-ARCHITECTURE.md. It governs ongoing
development; IMPLEMENTATION-REVIEW.md governs the separate comprehensive audit.
docs/SUBSYSTEM-INVENTORY.md is the living map of responsibilities and workflows,
not a declaration that they have passed an audit. AGENT-CONTRACT.md governs execution
and test integrity. The immutable catalogs remain the feature requirements.

## Continuous verification

Every new project-owned feature, substantive behavior change, public operation and
confirmed correctness correction requires appropriate automated verification as it
is developed. Design tests to discover incorrect behavior, exercise the production
operation, and follow the existing CMake/CTest registration, fixtures and test
conventions described in BUILDING.md. For each substantive change:

1. Identify correctness invariants, independently specified outcomes, applicable
   success, rejection, boundary and failure paths, transitions and interactions.
2. Determine which properties already have meaningful evidence and add focused
   tests for relevant gaps. Verify that they exercise the intended paths and that
   assertions establish independent expected behavior, not implementation guesses.
3. Run applicable focused and broader tests; investigate failures, correct confirmed
   defects and add regression coverage. Never weaken assertions or change expected
   behavior to accommodate an incorrect implementation.
4. Record meaningful gaps, evidence, scope and limitations; update the affected
   inventory entries and dependent interface/workflow verification when warranted.

Select methods according to actual behavior, architecture and consequences. As
applicable cover positive and negative behavior; boundaries and equivalence classes;
both outcomes of meaningful decisions and significant combinations of compound
conditions, including short-circuiting; valid and forbidden state transitions and
operation orderings; error propagation and cleanup; cancellation, interruption,
rollback, partial failures and resource exhaustion; malformed, contradictory and
adversarial inputs; concurrency, reentrancy and callback ordering; persistence,
transaction and recovery boundaries; cross-subsystem interactions; and complete
observable user workflows. Not every technique applies to every change. Derive
oracles where practical from specifications, standards, independent fixtures,
simple reference models or established reference implementations. Do not merely
reproduce the implementation in a test.

Stubs, disabled paths and dependency-free tests cannot establish real integration.
Test Vellum's interfaces to third-party libraries, without imposing project-owned
coverage obligations on third-party internals. Use real integrations, controlled
virtual devices and a virtual display for applicable GUI/output tests. Follow the
contract's native, Ubuntu Docker, sanitizer and integration acceptance duties;
record unavailable checks as unavailable, never as passed.

## Verification at every development stage

Build meaningful coverage alongside implementation throughout each stage, including
its existing and newly added production paths, rather than deferring verification
to a later stage. For each new piece of project-owned production code, aim for more
than 95% line and more than 80% compiler-reported branch coverage during development.
Apply those goals independently to each test suite's defined production-code scope,
each meaningful subsection, and the whole application. Define and report a suite's
scope explicitly; do not require an unrelated suite to execute the entire application
or combine suites to conceal a gap in one suite's responsibility. Whenever
a suite scope or subsection reaches 100% line coverage, pursue more than 95%
compiler-reported branch coverage in that scope. Investigate uncovered edges and record specific
evidence where the target cannot be reached without violating the language or
implementation contracts; do not fabricate paths or weaken tests to meet a number.
Measure and report that code separately where practical, using consistent instrumentation;
do not let existing code or aggregate totals conceal its uncovered paths. Investigate
each material gap, including feasible compiler-generated exception and cleanup edges,
and add resilient tests with independent oracles as appropriate. Record unmeasured,
unreachable, impractical or otherwise unverified paths and the evidence and reasons;
do not claim the targets or correctness when the measurement is unavailable. Apply
the same aspirations project-wide and assess coverage distribution for the stage's
affected subsystems and critical
components, and investigate missing data, unexercised decisions and failure paths.
Add and run focused positive, negative, boundary and adversarial tests and applicable
integration and workflow tests as gaps are identified. Prioritize security, data
integrity, ownership and recovery; do not let well-covered code conceal weak areas.

Before accepting a stage, reconcile its implemented scope, correctness invariants,
tests, independent oracles, relevant proof arguments and coverage evidence. Resolve
confirmed defects and relevant verification gaps, verify applicable interactions,
and document any justified limitations. A stage with missing relevant evidence is
unfinished, even if the application builds, tests pass or aggregate targets are met.
Apply the proof and measurement rules below; no numerical build gate is introduced.

## Coverage and verification principles

Measure line and compiler-reported branch coverage in CI; distinguish these from:

| Evidence | Meaning and limitation |
| --- | --- |
| Line coverage | A line executed; it does not establish the outcome or assertion quality. |
| Compiler branch coverage | Instrumented control-flow edges taken; GCC/gcov can include exception, cleanup, destructor and other compiler-generated edges. It is not source decision coverage. |
| Source decision coverage | Outcomes of meaningful source decisions; both outcomes alone do not establish each constituent condition. |
| Condition coverage | Conditions and relevant combinations, including short-circuit behavior; feasible combinations and semantic consequences still need analysis. |
| State-transition coverage | Exercised modeled transitions and orderings; the model and unobserved states may be incomplete. |
| Failure-path verification | Evidence for errors, cleanup, cancellation and recovery at specified failure points; uninjected failures remain unestablished. |
| Integration verification | Evidence at actual interfaces and subsystem boundaries; it does not automatically establish an entire user workflow. |
| End-to-end verification | Observable workflow evidence across participating components; it covers only exercised conditions and observable oracles. |

Use structural coverage diagnostically to locate gaps, not as proof of correctness
or as an independent optimization objective. Where practical supplement raw branch
counts with source-level decision and condition analysis. Identify compiler-generated
edges and justify unreachable defensive paths; do not automatically dismiss uncovered
edges. No number, including 100%, establishes semantic correctness. Do not add
trivial tests, weaken assertions, exclude relevant production code or invent execution
paths merely to raise a percentage. The existing numerical aspirations in the main
architecture are not mandatory build gates. A sound general proof can establish a
property without redundant exhaustive test enumeration, but it must not be used to
conceal a different unverified behavior or the absence of integration and regression
tests. Every reachable path without adequate evidence requires a specific tracked
verification task; do not claim feature completion while relevant evidence is missing.

Report project-wide, per-subsystem and important component-level line/branch results
when instrumentation is available, along with internal disparities, unverified critical
behaviors, exclusions, unavailable data and measurement limitations. High coverage in
one subsystem or low-risk code never compensates for a gap in another subsystem or a
critical component. Keep definitions and instrumentation stable for comparisons;
explain changes in revision, configuration, toolchain, workload, source mapping and
instrumentation. Prefer automatically generated reports from gcovr; document the
shared-source allocation and avoid adding overlapping subsystem totals. An aggregate
report does not satisfy per-subsystem assessment. See SUBSYSTEM-INVENTORY.md and
BUILDING.md for mappings, current tooling and reproducible reporting procedure.

## Subsystem and workflow maintenance

A major subsystem is a cohesive responsibility whose complexity, user-visible
behavior, security, integrity, ownership, lifecycle, failure modes, invariants or
interactions warrant independently identifiable verification, regardless of size.
Boundaries reflect behavior, not directories or coverage convenience. Every substantial
production component and supported feature must have an identifiable scope, including
shared infrastructure. Account for shared ownership explicitly; reconsider boundaries
instead of silently omitting an unclassified component. Avoid arbitrary fragmentation,
duplication and aggregation that hides important differences.

For each identified subsystem, measure/report available line and branch coverage,
interpret compiler-generated branches, identify source decisions and conditions,
invariants, failure paths, transitions, important components and evidence quality,
and prioritize gaps by risk. Evaluate critical internal behavior separately even when
subsystem totals are high. Record the responsible source areas, interfaces and
verification expectations in the living inventory; detailed evidence and outstanding
findings belong in the audit inventory and verification records, not duplicated there.

When development, review, testing or defect investigation reveals an omitted,
newly significant, extracted or changed subsystem, reconcile the affected inventory
entries with implementation: add, remove, merge or refine them, their invariants,
verification expectations, interfaces and workflows, and test affected behavior.
Do the same for substantial new user-visible workflows. Routine changes need only
reconcile affected areas, not the whole application. During a comprehensive audit,
reconcile the entire inventory against sources, architecture, interfaces and workflows,
and correct omissions, overlaps and obsolete classifications.

## Cross-subsystem and workflow verification

Assign responsibility for shared interfaces and invariants in the inventory. An
interface or shared-invariant change requires identifying affected dependents and
testing their interactions. Isolated coverage does not establish composition. As
applicable verify load/edit/save/reopen/recovery integrity; authorization across
operations; session identity, ownership and lifetime; agreement of live, rendered,
printed/exported and persisted representations; callback, cancellation and session
replacement ordering; propagated failures and cleanup; transaction atomicity; and
platform/build compatibility. These examples do not limit discovery of other risks.

For each significant supported workflow, identify initial conditions, participating
interfaces, independent oracles, observable outcomes and invariants; exercise relevant
success, rejection, cancellation and failure paths, feature/state combinations and
external effects. Inspect actual resulting documents, persistent state, signatures,
resource ownership and visible behavior where applicable, not merely a success code.
Mark planned workflows as planned rather than claiming unsupported functionality.
The workflow inventory evolves with the application.

## Risk and generalized correctness reasoning

Prioritize by consequences: security, corruption and data loss, signature integrity,
memory safety and undefined behavior, denial of service, resource ownership and
lifetime, transaction/recovery consistency and observable behavior. For critical
paths aim to exercise every feasible meaningful authorization decision, state and
ownership transition, commit/rollback point, error/cancellation outcome, boundary,
hostile-input rejection, failure point, relevant asynchronous ordering and interaction.
Choose practical evidence: unit/integration tests, boundary/equivalence partitions,
property or stateful model tests, differential and mutation testing, fuzzing, fault
injection, concurrency stress, sanitizers and static analysis. Prefer exhaustive
exploration of bounded models where tractable; do not apply techniques mechanically.

Where practical, establish properties over whole defined input or reachable-state
domains, including invalid and adversarial cases, using sound mathematical or
structural induction, loop/state invariants, symbolic execution, abstract interpretation,
model checking, deductive or mechanically checked verification as appropriate. For
induction establish base cases and the inductive step; a pattern in examples is not
a proof. Bounds, overflow, ownership, transitions, transactions, malformed input and
cryptographic protocol handling under stated primitive assumptions are examples of
potential properties. State the property, domain, preconditions, environmental and
concurrency assumptions, external dependencies, undefined/implementation-defined
behavior, resource limits, proof obligations and production correspondence. A model
proof is not automatically a proof of implementation. Distinguish mechanically checked
proofs, sound deductive arguments, conditional model proofs, informal reasoning,
heuristic analysis and test evidence. Challenge assumptions and correspondence with
targeted tests and counterexamples. A sound general argument is stronger than finite
sampling for precisely the property it proves; do not extrapolate beyond its scope.
Where rigorous general verification is impractical, record the limitation and use the
strongest practical combination of analysis, tests and independent validation. Formal
proof is not a universal prerequisite.

## Comprehensive audit integration

The later audit under IMPLEMENTATION-REVIEW.md independently reviews specifications,
invariants and oracles; reconciles the complete inventory; examines coverage distribution,
compiler-generated edges, critical decisions and failures, interactions and workflows;
identifies ineffective tests; evaluates opportunities for general proofs and their
assumptions, scope and implementation correspondence; remediates by risk; and records
remaining uncertainty. Aggregate structural coverage alone cannot close that audit.
Establishing or updating this verification architecture is not that audit.
