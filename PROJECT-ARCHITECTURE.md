# Vellum architecture — governing specification

## Product and requirements authority

Vellum is a GPLv3 C++23 desktop PDF reader progressing into a full PDF editor.
Vellum-owned code is GPLv3; MuPDF retains AGPL/commercial licensing and combined
distributions must honor those terms (docs/LICENSING.md). Linux is first; wxWidgets >=3.2.6 supplies the native UI, MuPDF supplies PDF
interpretation/rendering and eventually serialization, embedded V8 supplies
JavaScript, and wxWidgets supplies platform printing integration. Project subsystems are separate
static libraries. Statically linking these subsystems does not require a fully
static executable or static GTK/system libraries.

The two supplied catalogs in docs/ are immutable requirement catalogs (not JSON
Schema validation specifications). Raw supplied text is retained alongside JSON
with only Markdown code fences removed. Checksums guard both. Preserve IDs and
exclusions. The editor catalog defines ultimate scope; Reader exclusions apply
to the Reader milestone, not to editor features included in the full catalog.
Map AR IDs through source_feature_id to AC IDs; do not double-count shared work.
Catalog acceptance criteria are starting points: derive specific independent
oracles and adversarial acceptance cases before implementing each feature.

PROJECT-ARCHITECTURE.md and docs/PRINTING-ARCHITECTURE.md govern architecture. CODING-STANDARDS.md governs code;
AGENT-CONTRACT.md governs execution; IMPLEMENTATION-REVIEW.md governs audits.
Do not silently resolve a material contradiction. Keep durable work in the stage
plan and feature-status ledger, never by editing the catalogs. HANDOFF.md holds
only the active resumable operation. CHANGELOG.md records delivered changes under
the current release, newest releases first, starting at 0.34. Do not invent a new
release number for each agent turn; open a new section when a release is chosen.

## Dependency direction and ownership

| Static target | Responsibility | May depend on |
| --- | --- | --- |
| vellum_view_geometry | Portable dynamic zoom calculation | Standard library |
| vellum_document | Mode and revision state | Standard library |
| vellum_persistence | Atomic file publication and bounded reads | Linux/POSIX |
| vellum_recovery | Snapshot validation, timer policy, commit bookkeeping | document, persistence |
| vellum_pdf | MuPDF ownership, rendering, permissions, snapshots | persistence, static MuPDF |
| vellum_scripting_transport | Bounded control messages, shared memory, child lifetime | Linux IPC |
| vellum_scripting_runtime | V8 initialization, context and execution inside worker | V8, scripting transport |
| vellum_print_plan | Portable selection and sheet imposition | Standard library |
| vellum_printing | Custom dialog, preview and native submission | print plan, PDF, wxWidgets |
| vellum_ui | wxWidgets views and orchestration | Public subsystem APIs |

The above list is far from exhaustive and and as vellum grows in scope and capability
it is expected that other items will be added to this file in the above list.
The UI must not reach into MuPDF/V8 internals. MuPDF is exposed through an opaque
implementation; its longjmp exception mechanism must never bypass C++ objects
with destructors. Establish a C boundary with fz_try/fz_always/fz_catch, then
translate errors into C++ exceptions outside it. No V8 types cross the worker
process boundary. PDF and V8 headers are system includes; project warnings remain
errors. Dependencies are not subject to project formatting or coverage metrics.
Never include V8 or MuPDF source code in Vellum coverage totals; Vellum-owned
adapters and worker code remain in scope and require boundary tests.

Every project-owned callback passed to MuPDF must use a C-compatible, non-throwing
trampoline. Use `vellum::guard_callback` for scalar or void callbacks, or an
equivalent guard for other signatures: initialize exception storage explicitly empty,
retain the first C++ exception in operation-owned state, suppress subsequent ordinary callback work and return a failure
value valid for that specific MuPDF callback. Required cleanup callbacks must still
run despite a pending exception; `vellum::guard_cleanup_callback` retains the first
error while protecting such void callbacks. After MuPDF returns and its error boundary
has completed, report the retained exception without losing a separate MuPDF error
or skipping cleanup. If an API has no safe failure value, design an appropriate
abort or error protocol before using it. Keep callback state alive for every possible
invocation and synchronize it when callbacks can run concurrently. A callback must
not use MuPDF longjmp across C++ frames with live destructors; the C++ exception
guard alone does not contain MuPDF's longjmp mechanism.

Each open document has one owning session. Current bootstrap rendering is
synchronous on the UI thread and bounded by page dimensions; background document
work must be introduced as a single-owner document executor before scaling to
large documents. Never let multiple threads manipulate one live MuPDF document.
UI interaction remains UI-thread-only. Autosave serializes a coherent revision
on the document owner. Results carry session identity and revision/generation;
stale results are discarded after replacement/closure. Never share mutable PDF
objects across documents or let callbacks own their document/UI in a cycle.

No persistence journal or command replay is a crash-recovery mechanism. Future
undo/redo is an in-memory editing facility with explicit transaction boundaries,
not a second recovery engine. Add mutations only behind a common transactional
API that updates revision state after successful actual changes. Selection,
scrolling, navigation and zoom do not mark PDF content changed.

## Document modes and manual saves

Every newly opened document starts in Reader mode, including a recovered PDF.
A per-document one-time action enables Editor mode for that open session. There
is no transition back until close/reopen. This is an application policy, distinct
from PDF permissions, certification restrictions or document dirtiness.

Reader mode may eventually support forms, layer changes and other Reader catalog
operations. Preserve all such in-memory document changes. Reader mode MUST NOT
create recovery snapshots, even through forced test triggers or after changes.
Enabling Editor mode makes all previous document changes eligible for autosave;
it does not discard, reload or reset them. Enabling Editor mode alone is not a
content change and does not force a clean document to autosave.

Manual saving is independent of Reader/Editor mode. Filling forms, annotation
changes and general edits have distinct permission classes. Permitted form changes
must be saveable without general editing privileges and without Reader autosaves.
The bootstrap has no mutation UI: may_save currently accepts modify, form-fill or
annotation rights; before mutations exist, enforce rights for the actual mutation
classes and certification state. The PDF COPY permission means text/graphics
extraction, not copying a whole encrypted file. Byte-preserving Save As is allowed
and retains original encryption and restrictions. Saving over the original checks
save rights and rejects external source replacement. Saving under a new pathname
preserves the live document/session identity.

Print, Save, Save As and Enable Editor remain available when a document is open.
Only a prohibited action asks for the standard PDF owner password. A cancelled or
incorrect attempt leaves the document unchanged; incorrect credentials cause an
alert. Verify credentials on an independent candidate before authenticating the
live model. Successful owner authentication authorizes the session until close,
without reloading it; only the Editor action changes application mode. Opening
can separately require the user/open password and does not imply owner authority.
Failed or cancelled protected opens preserve the previously open document.
Manual save never advances the last-autosaved revision: that revision tracks the
snapshot, not the original file. A clean close may discard snapshots only after
an explicit safe policy is implemented and tested; bootstrap snapshots persist.

## Autosave and recovery protocol

Default interval is 60 seconds, configurable through autosave_controller's interval
configuration; the bootstrap UI uses the default. A later preferences UI must
persist changes without bypassing the same policy. Only Editor mode with a revision
newer than the last successful snapshot is eligible. All triggers check this
policy. The production timer calls tick(now); trigger_now(now) bypasses only the
elapsed-time check, never eligibility, serialization, validation or commit logic.
Use trigger_now for deterministic tests rather than sleeping for timer expiry.

Retain exactly one committed full PDF snapshot per open document session, stored
in a private directory separate from the original. The bootstrap uses a private
random session directory and snapshot.pdf. It does not implement discovery or
original-path metadata yet; explicit recovery opens the committed snapshot. Future
metadata must be committed with the snapshot in one atomic envelope, not in a
separately committed sidecar that can disagree with the PDF. Do not retain prior
snapshot generations or replay any sequence of editing operations.

Write a unique temporary file in the destination directory, with private file
permissions. Write all bytes; fsync the file; atomically rename onto the final
snapshot; fsync the directory. The rename is the visibility commit point. After
successful synchronization, record the captured snapshot revision. Only mark that
revision; changes occurring after capture stay dirty. Serialization/write failures
before rename preserve the previous committed snapshot and dirty revision. A
failure after rename may leave the new snapshot visible but durability uncertain;
report the failure, keep the revision dirty and allow retry. Never fall back to
an older generation. Autosave never overwrites the original document.

Recovery reads only the exact committed .pdf filename. Files with temporary suffixes
are ignored and never promoted, merged or replayed. If the committed snapshot is
missing, invalid or cannot be opened, recovery fails visibly. No fallback to the
original or temporary file constitutes successful recovery. Recovery validation rejects PDFs MuPDF reports as repaired and requires a nonzero
page count. Integration tests must challenge truncated/partially recoverable files
and structures only read lazily; opening a document does not validate every object.
Expand validation with supported operations rather than claiming all structures
are verified from a successful open; see verification gaps.

Atomic visibility/durability reasoning assumes a local filesystem implementing
POSIX rename and honoring fsync on file and directory, without external mutation
of the private recovery directory. Power-loss guarantees depend on the storage
stack; process-exit tests do not prove device-level crash behavior. Bound reads
to 512 MiB in the bootstrap; larger-document/streaming support is tracked work.

The bootstrap PDF adapter preserves original loaded bytes because it implements
no PDF mutation. Before adding forms, annotations or layer edits, replace snapshot()
with full serialization of the live model and test Reader changes surviving the
Editor transition, signature restrictions, failure atomicity and reopen equivalence.
Never claim original bytes represent an edited live document.

## JavaScript process and transport

Only vellum-script-worker links V8. The GUI/document side uses process IPC, not
an in-process interpreter. The bootstrap launches one disposable worker per
request; document-persistent contexts and Acrobat host APIs are later stages.
Requests/responses use a private inherited SOCK_SEQPACKET control socket and a
sealed-size memfd mapping, at most 1 MiB each. Control packets carry magic,
protocol version, bounded byte length and status. Binary fields are native-endian
because peers are same-machine same-build processes; never use this format for
network or cross-version persistent interchange. The worker copies source into
private memory before execution and the parent copies output before unmapping.

Do not send raw pointers, V8 handles, MuPDF handles, shared_ptrs or arbitrary native
objects. Shared memory carries bytes only. Future host operations use validated
schemas, stable session/object IDs, generations, capability checks, bounded
lengths and explicit request ordering. No script receives direct filesystem,
network, shell, printer or signing access. Privileged requests must be mediated
and permission-checked by the owning process; process separation is not authority.

Each request owns its process/socket/mapping. A deadline kills and reaps a stuck
worker; errors cannot reuse an abandoned worker or affect another request.
The parent/worker control exchange establishes ownership transfer of mapping
contents; neither side modifies the mapping while the other owns the payload.
Reject malformed packets, oversize lengths and invalid response status.

The bootstrap has no Node.js host APIs, sets no_new_privs and disables core dumps.
This is process isolation, NOT a complete OS sandbox: filesystem/network access
must be restricted with a tested Linux sandbox before automatically executing
untrusted document JavaScript. V8's internal sandbox/pointer compression are off
in the initial embedding ABI to avoid header/build flag ambiguity. This does not
replace the mandatory OS isolation work. V8 memory limits are partial; enforce
total process resource limits and test OOM behavior in the hardening stage.
No document JavaScript autoexecutes in this bootstrap.

## Rendering, UI and printing

Rendering returns owned RGB bytes; UI converts explicitly to wxImage. Validate
page indices, finite zoom and dimensions before allocation. Current bounds are
8192 pixels per raster axis. Viewer zoom is bounded by the zoom policy; only the
visible region is rasterized, with an explicit virtual-size limit. Print regions
are tiled at the selected physical output DPI (36–600); low-quality print rights
limit output to 150 DPI. These are resource policies, not feature parity. Preserve UTF-8 at filesystem/wx/V8 boundaries. Malformed PDF parsing and
heavy render cancellation need process/resource hardening before hostile files
are treated as safe. GUI tests use Xvfb plus actual event actions, not source grep.

Printing is platform independent through wxWidgets; Vellum has no direct CUPS API,
linker or development-package dependency. A platform may use its own printing stack
under wxWidgets. docs/PRINTING-ARCHITECTURE.md governs the full Acrobat-equivalent
custom dialog, device properties, range/parity selection, scale, N-up, booklet,
poster tiling, content options and automatically updating sheet preview. Preview
and final output consume the same immutable plan. Real-time preview must cancel or
discard stale generations and must never dirty the PDF. The bootstrap implements the custom dialog, page selection, scaling, N-up,
booklet padding/binding, poster overlap/marks, duplex/manual-duplex sequencing,
copies/collation, live preview and native submission. Actual annotation/widget
Print versus View flags are respected by the MuPDF usage-specific renderer.
Output freezes a job copy and disables UI changes while submitting. Physical
printer capabilities and advanced color/content/production options need further
acceptance; see docs/PRINTING-ARCHITECTURE.md. Keep planning independent of OS and
PDF-engine types.

## Build and verification boundaries

Continuous, risk-based verification is part of development, not a final acceptance
step. [docs/VERIFICATION-ARCHITECTURE.md](docs/VERIFICATION-ARCHITECTURE.md) is
the authoritative verification architecture; its living subsystem and workflow
inventory is [docs/SUBSYSTEM-INVENTORY.md](docs/SUBSYSTEM-INVENTORY.md). Apply
them to all project-owned behavior and update the inventory as the product evolves.
IMPLEMENTATION-REVIEW.md governs the separate comprehensive audit.

For every new piece of project-owned production code, aim for greater than 95% line
and greater than 80% compiler-reported branch coverage as it is developed, as well
as across the project. Measure the changed code separately where instrumentation
permits; investigate uncovered decisions and feasible compiler-generated paths and
add meaningful verification instead of leaving gaps for a later stage. Record
measurement limitations and any remaining gaps and their reasons. These targets do
not establish correctness by themselves or impose a numerical build gate. No high
aggregate result excuses an unverified critical path or subsystem. Account explicitly
for missing measurements and the scope and assumptions of correctness arguments.
Apply these goals to every newly created project-owned source file and each
meaningful subsection, as well as the whole application. When a subsection has
100% line coverage, pursue greater than 95% compiler-reported branch coverage
there. Investigate and document genuinely unreachable or otherwise unverified
edges; never fabricate behavior or weaken verification to meet a percentage.

Maintain `HANDOFF.md` as a concise current recovery checkpoint. Update it after
each source-file change and before the next action. Record the active state,
immediate next steps and genuinely unresolved blockers; remove completed results
unless they are needed to resume. Permanent requirements and durable findings
belong in the authoritative architecture or verification documents, not a diary
of completed work in the handoff.

Keep instrumented test counters and coverage-report generation on a RAM-backed
filesystem when available, preserving complete project and subprocess coverage.
Size its allocation from measured peak usage of the full suite, with headroom;
reassess and increase Docker and CI allocations as the application and tests grow.
Report insufficient space or counter-write failures rather than presenting partial
coverage as complete. Persist reports only when they are needed as evidence.

CMake builds separate static subsystems, executable UI and isolated V8 worker.
wxWidgets is a required system development installation at normal configuration.
No separate CUPS development dependency is required. MuPDF and V8 are built
and installed locally by prepare when compatible static installations are absent;
normal full builds depend on that preparation. Explicit roots require ABI/version
receipts; incompatible libraries are rejected rather than linked by accident.
No third-party sources/binaries are included in the source archive. Dependency
sources/builds stay in ignored work/. Preparation must be resumable and bounded
in concurrency. Never automatically install OS packages on the user's Slackware.

Core-only builds test domain/persistence/IPC policy without GUI/PDF/V8. Passing them
never establishes correctness of real MuPDF/V8/wxWidgets printing integration. Full CI prepares
real dependencies and runs integration tests. Coverage reports include line and
branch data; unresolved reachable-path verification gaps remain tracked. Audit the tests and
oracles themselves under IMPLEMENTATION-REVIEW.md. Record tool versions, commands,
source state and limitations in evidence, not just test counts. Never claim a
formal proof from a successful test run or an informal correctness argument.
