# Vellum — Comprehensive Adversarial Correctness Audit

## Mission

Conduct a comprehensive, adversarial correctness audit of Vellum, treating the existing implementation, tests, architectural assumptions, and previous verification results as potentially incorrect.

The objective is to establish the strongest practically achievable, reproducible evidence of correctness using all computationally tractable verification methods available in the current environment, including rigorous logical reasoning, mathematical proofs, and empirical verification.

**Seek counterexamples rather than confirmation.** Do not optimize merely for passing tests, producing a favorable report, or completing the audit quickly.

Follow `AGENT-CONTRACT.md`. Preserve all existing work, including staged and unstaged changes. Maintain `HANDOFF.md` as a concise, current-state recovery checkpoint.

This audit includes investigation, proof development, correction, regression testing, and re-verification. It is not complete merely because an assessment or report has been produced.

## Governing principles

1. **Specifications take precedence over implementation behavior.** Do not treat existing code or tests as authoritative merely because they agree with each other.
2. **Evidence takes precedence over assertions.** Every claimed verified property must be supported by identifiable, reproducible evidence, including sound deductive arguments where applicable.
3. **Correctness includes failure behavior.** Normal operation, boundary conditions, malformed inputs, partial failures, cancellation, and interruption are all within scope.
4. **Cross-subsystem interactions are first-class audit targets.** Correctness of individual components does not establish correctness of their composition.
5. **General proofs can establish properties without exhaustive testing.** Prefer sound inductive, deductive, or compositional arguments when they establish a property over an unbounded or impractically large state space.
6. **Proof assumptions must be justified.** A proof establishes implementation correctness only to the extent that its model, premises, environmental assumptions, and connection to the actual implementation are valid.
7. **Verification methods must be appropriate to the risks.** Select techniques for their ability to establish correctness or discover defects, not to satisfy a checklist.
8. **Uncertainty must be explicit.** Distinguish established properties, confirmed defects, unresolved risks, conditional proofs, and limitations of available evidence.
9. **Remediation must preserve correctness.** A fix is not complete until its relevant invariants have been reexamined and regression verification succeeds.
10. **The audit itself must be auditable.** Scope, coverage, exclusions, assumptions, proof obligations, decisions, findings, and results must be traceable.
11. **Intermediate progress is not completion.** Discovering a defect, completing a phase, updating the handoff, or reporting progress does not authorize ending the task.

Do not demand exhaustive enumeration where a valid general proof establishes the relevant property. Conversely, do not substitute informal intuition for proof or treat passing tests as proof of universal correctness.

---

## Phase 1 — Establish the independent correctness specification

Before relying on existing implementation assumptions or test expectations:

1. Identify all applicable specifications, architectural requirements, documented constraints, and intended observable behavior.
2. Distinguish explicitly specified behavior from behavior inferred from existing code or tests.
3. Identify ambiguities, contradictions, missing requirements, and assumptions lacking independent justification.
4. Reconcile requirements where authoritative specifications permit doing so.
5. Where a material ambiguity cannot be resolved independently, document the alternatives and consequences. Continue unrelated work rather than inventing requirements.
6. Derive correctness invariants independently of the implementation.
7. Identify properties suitable for deductive verification, including induction over operations, recursion, data structures, and state transitions.
8. Identify the assumptions and proof obligations necessary to establish those properties.

Include invariants covering both individual subsystems and interactions between them.

For every invariant, identify:

- Its authoritative specification or justification.
- The states and operations to which it applies.
- Conditions under which it must hold.
- Relevant failure and interruption behavior.
- What would constitute a counterexample.
- Whether it admits a general proof, empirical verification, or a combination.
- How the property could be verified independently.
- Any assumptions on which the verification depends.

Where possible, formulate invariants precisely enough to support inductive reasoning.

For a stateful subsystem, this may involve proving:

1. **Base case:** The invariant holds in every permitted initial state.
2. **Inductive step:** Every permitted transition preserves the invariant, assuming it held beforehand.
3. **Coverage:** The transition model includes every relevant operation, callback, failure, cancellation, and externally observable state change.
4. **Implementation correspondence:** The implementation actually conforms to the modeled transitions and assumptions.

A successful induction over a complete and accurate transition model establishes the invariant for every finite reachable execution, without enumerating every execution history.

Additional reasoning is required for properties involving infinite executions, progress, termination, fairness, or liveness.

**Phase deliverable:** A specification and invariant inventory, including unresolved ambiguities, proof obligations, assumptions, and their impact on verification.

Do not proceed on the assumption that the existing test suite correctly represents the intended behavior.

## Phase 2 — Construct the audit coverage inventory

Reconcile docs/SUBSYSTEM-INVENTORY.md and the verification requirements in
docs/VERIFICATION-ARCHITECTURE.md against the complete current implementation,
interfaces and workflows. Review project, subsystem and critical component coverage
distribution, raw compiler-generated branch edges and missing instrumentation; do
not infer decision coverage or semantic correctness from aggregate percentages.

Inventory the actual production source tree, public and internal interfaces, supported user operations, persistent state, and architectural subsystems.

Map the inventory against the specification and invariants established in Phase 1.

At minimum, account for:

- PDF parsing, object graphs, streams, encryption, and serialization.
- Document identity, ownership, aliasing, and lifetime.
- Embedded documents and cross-document isolation.
- Editing, transactions, undo/redo, and history branching.
- Digital signing, signature validation, permissions, and protected revisions.
- Persistence, save/save-as, autosave, recovery, and publication.
- Session creation, replacement, teardown, and asynchronous work.
- JavaScript integration, callbacks, reentrancy, and cancellation.
- Concurrency, synchronization, and shared resources.
- Error handling, cleanup, resource limits, and hostile inputs.
- User-visible operations and interactions between these systems.

Expand this inventory to include any additional subsystems discovered in the actual implementation.

For each subsystem and operation, record:

- Applicable correctness invariants.
- Relevant implementation paths.
- Dependencies and interactions.
- Significant failure modes.
- Planned verification techniques.
- Applicable proof obligations and assumptions.
- Current audit status.

Explicitly identify and justify exclusions.

The coverage inventory must account for properties established by general reasoning as well as those established through testing.

A sound proof may cover infinitely many states or executions, but its scope must be stated precisely. Do not confuse the number of source files inspected or tests executed with the strength or breadth of the established property.

**Phase deliverable:** A coverage inventory reconciled against both the source tree and the requirements.

Do not infer completeness solely from the tests executed, files inspected, proofs attempted, or defects discovered.

## Phase 3 — Systematically audit implementation correctness

Trace the implementation paths that can affect each invariant.

Examine normal execution, boundary conditions, error paths, cancellation, and partial failure.

Where practical, construct explicit correctness arguments showing why the relevant implementation paths preserve their invariants.

Prioritize:

### Identity, ownership, and lifetime

Examine object identity, aliasing, reference ownership, resource lifetime, destruction ordering, stale references, and cross-document isolation.

Use ownership invariants, lifetime arguments, and inductive reasoning over object creation, mutation, transfer, and destruction where appropriate.

### Concurrency and asynchronous behavior

Examine synchronization, memory ordering, reentrancy, callback ordering, thread affinity, cancellation, and session teardown.

Reason explicitly about happens-before relationships, permitted interleavings, linearization points where applicable, and whether invariants hold under all permitted concurrent executions.

A proof that assumes sequential execution is insufficient for concurrently accessible state.

### Transactions and state consistency

Examine transaction boundaries, atomicity, rollback, undo/redo consistency, branching, partial commits, and externally observable intermediate states.

Establish state invariants and prove their preservation across successful operations, failures, and rollback transitions wherever practical.

### Persistence and recovery

Examine save operations, filesystem errors, interruption during publication, crash consistency, recovery authority, and retry safety.

Distinguish logical transaction correctness from assumptions about filesystem durability, atomic replacement, synchronization, and hardware behavior.

### Signing and document security

Examine signature creation and validation, cryptographic operations, permission enforcement, encryption and crypt-filter selection, protected revisions, and document integrity.

Distinguish properties established by the application from properties delegated to cryptographic libraries, PDFium, the operating system, or external trust infrastructure.

### Memory and resource safety

Examine undefined behavior, integer overflow, bounds violations, resource exhaustion, denial-of-service conditions, and exception safety.

Use deductive bounds arguments where possible, including proofs of termination, recursion bounds, memory-growth limits, and arithmetic safety.

### Untrusted input

Examine malformed PDF structures, inconsistent references, cyclic graphs, compressed objects, hostile streams, and adversarial combinations of otherwise valid features.

Prove structural properties where possible, but test whether the implementation actually enforces the assumptions required by those proofs.

For each path, seek concrete counterexamples to the applicable invariants.

Where a general proof is feasible, identify its base cases, inductive steps, supporting lemmas, assumptions, and implementation correspondence.

Do not assume a path is correct merely because its existing tests pass.

**Phase deliverable:** An implementation-to-invariant audit mapping, with findings, correctness arguments, proof obligations, and evidence.

## Phase 4 — Independently evaluate existing verification

Audit the existing tests and test infrastructure before relying on their conclusions.

Look for:

- Assertions that are too weak or tautological.
- Tests that duplicate implementation assumptions.
- Missing negative and boundary cases.
- Tests that pass without exercising the intended path.
- Fixtures that fail to represent realistic or adversarial inputs.
- Helpers or mocks that conceal failures.
- Shared defects between production code and test infrastructure.
- Tests whose claimed coverage exceeds what they establish.
- Nondeterministic tests and failures hidden by retries.
- Inadequate checks of externally observable behavior.

Where possible, derive expected results independently from specifications, reference models, external standards, mathematical properties, or separately constructed fixtures.

Use mutation testing where useful to identify ineffective assertions and missing coverage.

Where tests are intended to validate properties already established by reasoning, determine whether they meaningfully test the correspondence between the proof assumptions and the implementation.

Do not require an enormous number of examples to establish a property already covered by a sound general proof. Instead, use targeted tests to challenge the proof's assumptions, modeling fidelity, and implementation correspondence.

Do not weaken existing tests merely to obtain passing results.

**Phase deliverable:** A test-oracle assessment identifying reliable evidence, inadequate verification, and necessary corrections.

## Phase 5 — Design and execute adversarial verification

Select verification methods according to the implementation architecture, identified failure modes, and available opportunities for deductive reasoning.

Use all techniques that are feasible, computationally tractable, and likely to materially improve confidence.

Candidate methods include:

- Mathematical induction over states, transitions, recursion, and data structures.
- Deductive proofs using preconditions, postconditions, invariants, and supporting lemmas.
- Compositional reasoning about interacting subsystems.
- Symbolic execution and satisfiability-based verification where practical.
- Model checking and bounded-state exploration.
- Boundary-value and equivalence-class testing.
- Property-based testing with independently derived properties.
- Stateful model-based testing against simplified reference models.
- Differential testing against independent implementations.
- Mutation testing.
- Coverage-guided fuzzing of parsers, object structures, APIs, and state transitions.
- Fault injection at allocation, filesystem, transaction, callback, and publication boundaries.
- Concurrency stress testing and systematic interleaving exploration.
- AddressSanitizer, UndefinedBehaviorSanitizer, ThreadSanitizer, and other applicable instrumentation.
- Static analysis and compiler diagnostics.
- Exhaustive enumeration of bounded state spaces.
- Formal or semi-formal reasoning about critical invariants and state machines.

Do not use a technique merely to satisfy this list.

**Prefer general deductive verification when it can establish a property more completely and efficiently than finite testing or exhaustive enumeration.**

However, a proof about an abstract model does not automatically establish correctness of the implementation. Verify that the model accurately represents the relevant code paths, including failures, concurrency, and external dependencies.

Where a technique is unavailable, incompatible, or prohibitively expensive, document the limitation and evaluate practical alternatives.

### Deductive and inductive verification

For properties suitable for logical proof:

1. State the property precisely.
2. Identify the assumptions and required preconditions.
3. Establish the base case or initial-state conditions.
4. Prove preservation under every relevant operation or transition.
5. Establish any necessary termination, progress, or resource-bound properties separately.
6. Justify that the modeled operations cover the actual implementation.
7. Challenge the proof with adversarial cases and attempts to construct counterexamples.
8. Record the proof, assumptions, limitations, and supporting implementation evidence.

Use mathematical induction where appropriate, including structural induction for recursive data structures and transition induction for stateful systems.

For concurrent systems, account for interference from other threads and callbacks. Use suitable reasoning about atomic operations, synchronization, memory ordering, and permitted interleavings.

For compositional proofs, explicitly establish the assumptions each component makes about its environment and verify that interacting components satisfy those assumptions.

Do not label an informal plausibility argument as a proof. Distinguish:

- Machine-checked formal proofs.
- Rigorous mathematical arguments reviewed against the implementation.
- Conditional proofs about abstract models.
- Informal reasoning that suggests correctness but leaves proof obligations unresolved.

A valid proof can eliminate the need for exhaustive testing of the property it establishes, but it does not eliminate the need to verify its assumptions or the implementation's correspondence to the proof.

### Stateful subsystem verification

For every important stateful subsystem:

1. Identify states and permitted transitions.
2. Identify invalid states and forbidden transitions.
3. Enumerate operations, callbacks, failures, and cancellation points.
4. Examine combinations and orderings capable of violating invariants.
5. Construct inductive proofs of invariant preservation where feasible.
6. Construct focused tests for uncovered transitions and interactions.
7. Minimize discovered failures into reproducible regressions.

For bounded models, exhaustively explore reachable states where doing so adds meaningful evidence.

For larger systems, prioritize inductive invariants, compositional reasoning, dependency analysis, partial-order reduction, pairwise or higher-order interaction testing, and risk-based exploration.

Do not assume passing existing transition tests establishes correctness of untested orderings.

Do not exhaustively enumerate states when a sound induction establishes the relevant property over the full reachable state space.

### Crash and transaction consistency

For every persistence or session-boundary operation, explicitly establish:

- The point at which the operation becomes committed.
- The authoritative state before and after commitment.
- Ownership of resources, callbacks, and asynchronous work.
- Failures that must preserve the old state.
- Failures that must preserve the newly committed state.
- Operations that may safely be retried.
- Intermediate states that must never become externally observable.

Where possible, prove that the transaction invariants hold before and after every permitted transition, including failure and recovery transitions.

Inject failures before, during, and after critical transitions wherever practical.

Verify that failures cannot silently lose user data, corrupt persistent documents, invalidate protected signatures, leak session-owned work into another document, or leave the application inconsistent.

**Phase deliverable:** Reproducible verification results, correctness proofs and their assumptions, newly discovered defects, and regression tests integrated consistently with the existing test architecture.

## Phase 6 — Remediate findings and re-verify

Classify findings as:

1. **Confirmed correctness defect:** Demonstrable violation of a requirement or invariant.
2. **Specification defect or ambiguity:** Missing, contradictory, or insufficiently defined behavior.
3. **Verification gap:** A property for which existing evidence is inadequate.
4. **Plausible unresolved risk:** A credible failure mechanism requiring further investigation.
5. **Verified property:** A property supported by identifiable empirical or deductive evidence.

For confirmed in-scope defects:

1. Establish the root cause.
2. Identify all affected invariants and implementation paths.
3. Design a correction consistent with the architecture.
4. Implement the correction.
5. Add meaningful regression coverage.
6. Run focused verification.
7. Reestablish any proofs whose assumptions or implementation correspondence changed.
8. Examine whether the correction introduces new failures or invalidates earlier evidence.
9. Repeat applicable broader verification.

For plausible risks, investigate until they are resolved, supported by evidence, or demonstrably limited by available verification methods.

A risk may be closed by a sound general proof, by sufficient empirical evidence under explicit closure criteria, or by establishing that the suspected failure mechanism is impossible under justified assumptions.

Define and apply explicit closure criteria. Do not endlessly revisit hypothetical risks after those criteria have been satisfied.

Do not conceal unresolved findings by reclassifying them without evidence.

**Phase deliverable:** A disposition for every finding, with correction evidence, proof evidence, or explicitly characterized residual uncertainty.

## Phase 7 — Independent adversarial review

Where practical, conduct independent reviews of critical subsystems using fresh analysis contexts that do not inherit the implementation agent's conclusions.

Provide reviewers with applicable specifications, relevant source code, and interfaces, but initially withhold the implementation agent's reasoning and claimed correctness arguments.

Require reviewers to independently:

- Derive relevant invariants.
- Identify potential counterexamples.
- Challenge implementation assumptions.
- Evaluate failure behavior and subsystem interactions.
- Construct independent correctness arguments where feasible.
- Challenge proof assumptions, completeness, and implementation correspondence.
- Identify weaknesses in existing verification evidence.

Compare independent findings against the primary audit.

Investigate disagreements rather than automatically accepting either conclusion.

Where independent reviewers derive different proofs of the same property, compare their assumptions and proof obligations. Agreement is meaningful only to the extent that the arguments are sound and their dependencies are understood.

For security-critical or especially complex subsystems, prioritize external tools, independent reference implementations, independently constructed fixtures, and separately derived test oracles.

Agreement between multiple AI reviewers is not proof of correctness, especially when they may share reasoning patterns or training-derived assumptions.

Any new confirmed defect or verification gap discovered during independent review must enter the remediation and verification process.

**Phase deliverable:** Independent review findings, independently derived correctness arguments, reconciled disagreements, and resulting corrections or residual risks.

## Phase 8 — Final verification and reproducible evidence

After corrective changes, repeat all applicable:

- Native build and test verification.
- Docker build and test verification.
- Sanitizer verification.
- Integration and end-to-end verification.
- Release-build latency and performance verification.
- Security, fuzzing, fault-injection, and state-model verification selected during the audit.
- Revalidation of deductive proofs affected by implementation changes.

Record the exact source revision and relevant working-tree state, build configurations, commands, tool versions, results, and evidence locations.

Maintain a traceable mapping between:

- Every correctness invariant.
- Relevant implementation paths.
- Verification techniques selected.
- Verification evidence obtained.
- Proof statements, assumptions, and supporting arguments.
- Confirmed defects and their dispositions.
- Remaining limitations and uncertainty.

Distinguish empirical verification from deductive proof.

For deductively established properties, record the proof's scope, assumptions, and relationship to the actual implementation.

Do not claim machine-checked formal verification unless a formal verification method was actually applied successfully.

Do not claim that a property is verified when the evidence only establishes a weaker property.

**Phase deliverable:** A reproducible evidence manifest, final verification results, and documented correctness arguments.

## Phase 9 — Audit completeness and closure

Before declaring completion, reconcile the final audit coverage inventory against:

1. The production source tree.
2. The applicable specifications.
3. Supported user operations.
4. Public and internal interfaces.
5. Cross-subsystem dependencies and interactions.
6. All discovered findings and their dispositions.
7. The final verification evidence.
8. All proof obligations and assumptions relied upon.

Identify any omitted paths, uncovered invariants, untested interactions, unresolved proof obligations, or unresolved findings.

For every exclusion or remaining limitation, record the justification and potential correctness impact.

The audit is complete only when:

- Relevant implementation paths have been systematically examined.
- Applicable invariants have been independently evaluated.
- Selected tractable verification methods have been executed.
- General proofs have been developed where practical and their assumptions justified.
- Confirmed in-scope defects have been corrected and re-verified.
- Independent review findings have been reconciled where practical.
- The coverage inventory has been reconciled against the actual implementation and requirements.
- Remaining uncertainty has been explicitly characterized.
- The final verification evidence is reproducible.
- No known unresolved in-scope correctness defect remains.

Do not require exhaustive enumeration of an unbounded or enormous state space where sound deductive reasoning establishes the relevant properties.

Conversely, do not claim that a property has been proved merely because no counterexample was found.

Do not declare completion merely because one audit pass found no additional defects or because all existing tests pass.

If a requirement cannot be verified adequately with available methods, characterize that limitation rather than inventing confidence.

---

## Execution and continuation contract

Execute the audit autonomously under `AGENT-CONTRACT.md`.

The phases define required coverage and evidence, not permission to stop after each phase. Work may proceed iteratively where dependencies require it; discoveries in later phases must trigger appropriate earlier analysis and re-verification.

Maintain `HANDOFF.md` as a concise, current-state recovery checkpoint containing:

- The current audit phase and active task.
- The most recent established findings.
- Any unresolved technical questions or proof obligations.
- The next concrete executable action.
- Relevant verification status.

Keep permanent specifications, audit evidence, detailed findings, and correctness proofs in appropriate persistent project documentation rather than allowing `HANDOFF.md` to become an ever-growing report.

**Continue working until the audit satisfies its completion criteria or a genuine technical or enforced execution barrier prevents further progress.**

A difficult defect, failed test, unresolved investigation, incomplete proof, completed intermediate task, progress report, handoff update, or approaching context limit is not by itself a reason to stop while further tool execution remains possible.

When encountering an obstacle, investigate whether it actually prevents further work. If it does not, continue with the next feasible action.

Do not substitute explanations of intended work for executing that work.

If an enforced execution limit interrupts the task, preserve a precise recovery checkpoint so the next session can resume without repeating completed investigation.

The objective is not to produce a declaration that Vellum is correct. It is to establish the strongest practically achievable evidence of correctness, correct the defects discovered, and document precisely what has and has not been established.
