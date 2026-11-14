# Ordered autonomous development stages

A stage is a substantial unit of implemented and verified work, not permission to
stop the user's broader task. Resolve the active HANDOFF first; then select the
first unfinished prerequisite stage. Recursively break difficult work into concrete
subtasks. Preserve catalog exclusions and dependencies; never modify source
catalogs to mark progress. Review every stage under IMPLEMENTATION-REVIEW.md.
Keep this document focused on remaining work: remove completed requirements as
their acceptance is established, down to individual sentences or clauses where
appropriate. Revise or expand the remaining tasks when investigation reveals
additional work. Preserve partially met requirements and their outstanding
acceptance conditions; passing a narrow test alone does not complete them.

## Stage 0 — Build/integration acceptance (first builder task)

Establish remaining platform and configuration acceptance independently. Fix any
remaining upstream API/build mismatches and extend generated PDF and wx event
tests to cover outstanding integration paths. Verify interrupted preparation,
installation, and missing/incompatible roots.
Reconcile the audit inventory and line/branch coverage against every production
source path. Extend worker coverage beyond the direct protocol tests and create fault-injection
seams for reachable uncovered OS failures.
This stage is unfinished until GUI/MuPDF/V8/native-printing behavior is tested with real libs.

## Stage 1 — Complete viewer workspace and single-owner execution

Extend the implemented single-document navigation and dynamic zoom/fit with
tabs/windows, navigation history, rotation, scrolling modes,
thumbnails, bookmarks, destinations, selection/copy, document properties,
preferences and UTF-8/keyboard flows. Move PDF work to a document-owned executor
with cancellation and generation-tagged results; guarantee close/reopen cannot
publish stale work. Cover actual UI operations under Xvfb, generated and independent
PDF fixtures, malformed inputs, large pages, allocation failures and teardown.
Map completed AR/AC IDs only when their full catalog acceptance criteria are met.

## Stage 2 — Live document state, forms and recovery composition

Before any PDF mutation tool, serialize the live model into a complete snapshot.
Implement a common permission-aware transaction API and revision bookkeeping;
no recovery command replay. Implement AcroForm types, appearances, layers,
Reader-authorized changes, exports and manual save/copy with explicit session
identity. Test Reader changes with zero autosaves, one-way Editor transition,
retention of all pre-transition changes and forced/timed autosave equivalence.
Implement configurable autosave preferences, recovery discovery/identity in an
atomic envelope, clean-close retention policy and strict non-repair validation.
Inject disk-full, short-write, fsync, rename, corruption, external replacement,
permission and process-crash faults. Assert original bytes are unchanged, only the
latest committed snapshot is authoritative, and invalid recovery fails visibly.

## Stage 3 — Persistent JavaScript sessions and Acrobat Reader scripting

Replace disposable expression sessions with one isolated context per document.
Define versioned capability-scoped IPC host operations, shared-memory ownership,
IDs/generations and callback/event order. Implement Adobe-compatible forms and
calculation/validation/formatting semantics incrementally with independent oracles.
No shared pointers or native handles cross IPC. Test reentrancy, script-driven
save/sign/close transitions, cancellation, timeout/OOM, worker crash, large payloads,
malformed messages, permissions, cross-document isolation and stale replies.
Apply tested OS filesystem/network/resource isolation before enabling automatic
execution of untrusted document scripts. Test sandbox escape attempts and broker
capability rejection; V8's internal sandbox is a separate consideration.

## Stage 4 — Reader annotations, printing, search and accessibility

Implement Reader comments/markup, undoable annotations, text search, attachments,
layer UI, accessible controls/read order and the full portable printing subsystem.
Extend the implemented custom range/parity selection, scaling, N-up, booklet,
duplex, poster tiling/overlap/marks and automatic shared-plan preview to complete
docs/PRINTING-ARCHITECTURE.md, including content options, presets and production
requirements. Retain regression coverage of the existing operations.
Use wxWidgets for native printer selection/properties and output, not a direct
CUPS dependency. Test actual control changes, preview generations, sheet geometry,
capability changes, cancellation and device errors using independent plan oracles,
fake devices and controlled virtual printers; no physical printouts. Test
Unicode/complex scripts, form focus, accessibility keyboard navigation, printing
permissions, spool errors, annotations saved/reopened and PDF coordinate transforms.
Include negative/adversarial interactions with scripts, signatures and recovery.

## Stage 5 — Signatures, certification and trust

Implement certificate verification/signing through a dedicated crypto/trust adapter,
protected revisions, timestamps and permission/certification interactions. Document
exact trust semantics. Preserve signed byte ranges and never silently invalidate
signatures during recovery/copy. Establish a tested session replacement protocol
for irreversible saves/signing while scripts/callbacks are active. Cover cancel,
missing credentials, untrusted/revoked chains, malformed signatures, offline trust,
write failures and worker failure. No replay-based recovery or implicit history
entry should stand in for a committed signed session transition.

## Stage 6 — Reader catalog closure

Reconcile all included Reader IDs, advanced/conditional decisions and exclusions.
Finish supported portfolios/multimedia/technical PDF/accessibility behavior with
explicit dependency/platform decisions. Complete full matrix, performance budgets,
security fuzzing, resource stress and independent audit; no placeholder completion.
Document remaining conditional gaps accurately rather than weakening criteria.

## Stage 7 — Editor core and authoring

Deliver atomic edit transactions, coherent undo/redo branching, page operations,
content/text/image editing, annotation/form authoring, redaction and sanitization.
Redaction must remove sensitive content rather than cover it visually. Verify
save/reopen equivalence, protected revisions, geometry/font/Unicode correctness,
rollback, cancellation and recovery composition. Split major new subsystems into
static libraries. Preserve every Reader-mode invariant.

## Stage 8 — Remaining full editor catalog, print production and portability

Implement remaining included AC features in dependency-ordered batches (conversion,
OCR, optimization, comparison, accessibility remediation, preflight and automation).
Finish included print-production/color/preflight requirements in the printing
specification and catalog rather than stopping at basic Reader print options.
Select additional libraries only with recorded rationale/license compatibility.
Vendor-only exclusions remain exclusions; never imply unavailable proprietary
services. Add Windows/macOS filesystem, printing, IPC, sandbox and packaging
backends behind stable interfaces and rerun platform-specific correctness audits.

## Feature acceptance procedure

For each selected feature: record IDs, independent intended behavior and interaction
invariants; implement actual user paths; derive positive/negative/adversarial and
failure oracles; run unit/integration/UI tests, applicable sanitizers, coverage and
Ubuntu Docker; review correctness arguments; record evidence. Only then set status
complete and add the delivered change to the current CHANGELOG section. A partial
implementation remains partial, even if its narrow tests pass.

Apply the project-wide testing and coverage requirements in
PROJECT-ARCHITECTURE.md and the stage verification procedure in
docs/VERIFICATION-ARCHITECTURE.md to every stage and feature. Do not defer relevant
coverage gaps in a stage's implemented scope to a later stage as a substitute for
acceptance.
