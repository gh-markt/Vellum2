# Living subsystem and workflow inventory

This is the architectural verification-scope map required by
VERIFICATION-ARCHITECTURE.md, not an audit closure or feature-completion ledger.
It is based on PROJECT-ARCHITECTURE.md, PRINTING-ARCHITECTURE.md,
DEVELOPMENT-STAGES.md and the current source tree. “Bootstrap” means an existing
implementation path, not full feature acceptance. “Planned” means explicitly
specified future work, not working functionality. Catalog details and exclusions
remain authoritative; feature-status.json tracks catalog acceptance. Reconcile
affected entries when functionality or responsibilities change. This initial map
is not presumed exhaustive. Detailed findings/evidence are in AUDIT-INVENTORY.md,
CORRECTNESS-ARGUMENTS.md and ../VERIFICATION.md.

## Current implementation responsibilities

Each row identifies a responsibility, its primary source areas/interfaces, and
verification scope. Header counterparts in include/vellum/ define public APIs.
Shared files may appear in multiple rows for semantic responsibility; the reporting
ownership map below assigns each production source to one primary group.

| Subsystem and status | Source areas and interfaces | Verification responsibilities and important gaps |
| --- | --- | --- |
| Application and session UI — bootstrap | src/app.cc, main_frame*.cc, main_frame.hh; wx events and document session | Startup/open/replacement, Reader-to-Editor, close, UI ownership, cancellation, keyboard/UTF-8 and failed-open preservation; real Xvfb tests include clean exit and a missing-file alert. Other entry-point failures remain unverified. Asynchronous document executor is planned. |
| Document state, permissions and session identity — bootstrap | document_state.cc, main_frame_permissions.cc, pdf_document.cc; mode/revision/owner authentication APIs | One-way mode, revision ordering and session-scoped authorization, rejected/cancelled operations and ownership. Mutation-class and certification enforcement are future requirements; check session/UI/PDF boundaries. |
| PDF loading, structure and MuPDF integration — bootstrap | pdf_document.cc; opaque PDF API, MuPDF C exception/ownership boundary | Open/encryption/authentication, page and input bounds, malformed/repaired files, lazy object access, copy/extraction and error/lifetime handling. Broad hostile corpus, resource faults and complete lazy validation remain open. |
| Rendering, view geometry and resource limits — bootstrap | pdf_document.cc, view_zoom.cc, main_frame*.cc, geometry.hh; owned RGB, viewport and wx image boundary | Geometry/zoom, transforms, bounded allocations and virtual sizes, View intent, output pixels and teardown; malformed and extreme documents, large allocations and cancellation. Fonts/images/graphics are delegated to MuPDF; test Vellum's integration, not third-party internal coverage. |
| Persistence and atomic file publication — bootstrap | atomic_file.cc/.hh; bounded reads, temporary write, rename and fsync | Bytes and identity, permissions, short writes/EINTR/disk-full/fsync/rename failures, cleanup, old/new visibility and filesystem assumptions. Fault seams for reachable OS failures remain open. Shared with PDF saving and recovery. |
| Recovery and revision policy — bootstrap | autosave.cc/.hh, document_state.cc, atomic_file.cc, PDF snapshot/validator boundary | Reader suppression, timer/forced equivalence, captured revision and retry, rename commit, corruption, interruptions and original preservation. Live serialization, discovery/identity and recovery composition are planned; do not substitute the small test validator for PDF validation. |
| Scripting IPC and worker lifecycle — bootstrap | script_process.cc/.hh, script_worker.cc; bounded packets, memfd, spawn/deadline/reap | Valid/malformed packets, peer failure, timeouts, bounded ownership, cleanup, generation/isolation as applicable; syscall faults and FD inheritance remain open. Each current request uses a disposable worker. |
| V8 runtime and script security boundary — bootstrap, hardening planned | script_runtime.cc/.hh, script_worker.cc, script_process.cc; V8 and process boundary | Real V8 expression success/errors, malformed worker requests, isolation and resource behavior; no Node APIs. A direct protocol test permits normal worker exit and coverage flushing; the ordinary parent still kills the worker. OS sandbox, total resource caps, persistent contexts and host APIs are not implemented. |
| Page selection and sheet planning — bootstrap | print_plan.cc, sheet_layout.cc, print_plan.hh, geometry.hh | Independent page-union/parity and placement oracles, bounds, N-up, booklet, poster, duplex/copies, invalid and adversarial combinations. Physical printer fidelity and broader property/fuzz evidence remain open. |
| Printing preview, rendering and native output — bootstrap | print_dialog.cc, sheet_painter.cc, printing.cc and corresponding headers; immutable plan, PDF renderer and wx printing interfaces | Actual controls, preview/output consistency, print permissions and intent, geometry, cancellation, device errors and output; test controlled virtual devices, not physical printouts. Property roundtrips, spool failures and capability changes remain open. Full production printing is planned in PRINTING-ARCHITECTURE.md. |
| Dependency preparation, build, installation and platform integration — bootstrap | CMakeLists.txt, cmake/, scripts/prepare_dependencies.py, docs/dependencies.lock.json, Dockerfile, CI; static archive/ABI and OS boundaries | Compatible/pinned roots, fresh/reused/offline and interrupted preparation, install, runtime and platform/compiler/configuration behavior, real MuPDF/V8/wx integration. Fresh Ubuntu preparation and some failure paths remain unverified. Linux is the current backend; other platforms are planned. |
| Requirements and test infrastructure — active shared infrastructure | scripts/check_catalogs.py, scripts/make_fixture.py, scripts/make_security_fixtures.py, tests/, docs catalogs and feature-status.json, CMake/CTest | Catalog byte integrity, fixture provenance and independent oracle quality, test registration and capability limitations. Test harnesses are not production proof; security fixtures are supplied independently of MuPDF writing. |

## Explicitly planned responsibilities

These represent architectural scope, not delivered capability. Refine or split
them as their invariants and ownership become concrete; no list is closed.

| Planned subsystem | Architectural scope and verification expectations |
| --- | --- |
| Live editing, transactions and history | Stage 2/7 common permission-aware mutations, complete live serialization, revision and undo/redo branching; verify atomicity, rollback, save/reopen, Reader transition and signatures. No command-replay recovery. |
| Forms, annotations, layers and interactive elements | Stages 2/4/7 AcroForms, appearances, Reader modifications and annotation/layer UI; verify permissions, calculations, output, persistence and interactions with scripts, recovery and signatures. |
| Persistent scripting and asynchronous document execution | Stages 1/3 document-owned executor, persistent contexts, host capabilities and callbacks; verify session/generation identity, ordering, reentrancy, stale replies, sandboxing, cross-document isolation and teardown. |
| Cryptography, signatures, certification and trust | Stage 5 dedicated trust adapter, protected revisions and signing; verify byte ranges, permissions, trust/revocation/offline and failure behavior against independent oracles. |
| Viewer navigation, text, accessibility and preferences | Stages 1/4/6 tabs, navigation, selection/copy/search, assistive behavior, preferences and persistent UI state; verify Unicode, focus, permissions, keyboard flows and platform integration. Existing basic navigation/zoom is in the bootstrap UI/rendering rows. |
| Embedded documents and cross-document isolation | Stage 6 portfolio/embedded-document decisions and document/session ownership; verify nested identities, permissions, resources and isolation as capabilities are selected. |
| Import, conversion, export and interoperability | Stages 4/8 supported attachment/output, conversion and external-format boundaries; verify actual resulting artifacts, malformed input, external interfaces, data preservation and platform compatibility. PDF copying and printing already have bootstrap scope above. |
| Editing media, fonts, content and redaction | Stages 7/8 page/text/image authoring, redaction, OCR and optimization as catalog requires; verify layout, font/Unicode and geometry correctness, removal of redacted content, permission, recovery and output integrity. |
| Print production and color | Stage 8 plus PRINTING-ARCHITECTURE.md: color, preflight and remaining production features; verify actual output, independent geometry/color oracles and device capability/failure behavior. |
| Portability, packaging and deployment | Stage 8 Windows/macOS backends, IPC, sandbox, storage and native printing; verify build and behavior on each supported platform and migration/compatibility boundaries. |

## Cross-cutting invariants and significant workflows

The primary responsibility for each workflow is shared by the rows named below;
changes at an interface require re-evaluating all participating rows. For each
workflow, tests must establish initial state, independent expected observations,
invariants, relevant feature combinations and success/rejection/cancel/failure
outcomes. Concrete operations and failure points must be refined as capabilities
are implemented; a row is not a claim that all combinations have been tested.

| Workflow and status | Participants, initial conditions, observable evidence and verification scope |
| --- | --- |
| Open, authenticate, navigate, render, close/reopen — bootstrap | Application UI, session/permissions, PDF and rendering. Start with ordinary, encrypted or invalid documents; inspect visible page/geometry and session identity, permissions and retained previous state on failure/cancel. Use independent PDF/color fixtures and actual UI events; test malformed input, errors and teardown. |
| Protected copy/manual save — bootstrap partial | UI, permissions, PDF and persistence. Start from permitted and restricted documents; verify rejection/owner prompts and byte-preserving copy, original preservation and session identity. Live mutations and certification-aware saves are planned. |
| Reader changes, Editor transition, autosave and recovery — policy bootstrap; live editing planned | State, UI, PDF, recovery and persistence. Establish Reader/Editor and revision state; verify no Reader snapshot, captured revision and atomic old/new snapshot on failure/retry, strict recovery and no original overwrite. Real live mutation serialization and discovery are not implemented. |
| Plan, preview and print — bootstrap partial | UI, permissions, PDF rendering, plan and native output. Start with document/device settings; inspect independent geometry/pixels and controlled PostScript/device output, Print intent, cancellation and permission errors; capability/spool and physical fidelity remain open. |
| Isolated expression execution — bootstrap | IPC lifecycle and V8 runtime. Start with separate requests; verify result/error, timeout/reap, bounded messages and isolation using actual worker plus independently constructed malformed peers. It is not document-script autoexecution or an OS sandbox. |
| Edit, undo/redo, save/reopen and recover — planned | Live transactions/history, permissions, persistence/recovery, rendering and UI. Verify document contents and state across success, rollback, interruption and mixed features; Reader retention and signed revisions must survive correctly. |
| Forms/annotations/layers with scripts and session replacement — planned | Interactive elements, persistent scripting, executor, UI, permissions and session ownership. Verify event ordering, cancellation, stale results, cross-document isolation, saved appearances and cleanup under failures. |
| Sign, save, reopen and validate — planned | Crypto/trust, PDF, transactions, persistence/recovery and UI. Independently validate signed bytes, certification restrictions, trust and failure behavior, including offline and interrupted cases. |
| Search/selection/accessibility and export/conversion — planned | Viewer, text, accessibility, PDF and external-format integration. Verify observable text, focus and permissions, independently inspect artifacts, and test malformed/Unicode cases. |

These scopes also carry cross-cutting obligations for resource exhaustion and
denial-of-service resistance, security decisions, callback/ownership lifetime,
error propagation, persistent representation consistency and supported platforms.
No isolated subsystem test or successful final return code establishes the complete
workflow.

## Coverage ownership and baseline limitations

Use one primary source owner for non-overlapping structural totals: application/UI
owns app.cc and main_frame*.cc; state owns document_state.cc; PDF owns pdf_document.cc;
rendering geometry owns view_zoom.cc; persistence owns atomic_file.cc; recovery owns
autosave.cc; IPC owns script_process.cc; V8 owns script_runtime.cc and script_worker.cc;
planning owns print_plan.cc and sheet_layout.cc; printing owns print_dialog.cc,
sheet_painter.cc and printing.cc. Header/template code must be attributed to the
responsible subsystem if instrumentation reports it. Semantic verification of shared
files remains the responsibility of every affected subsystem above. Build scripts,
fixtures and configuration have verification scope even when C++ gcov data does not
measure them. Revise this map when implementation changes; do not add overlapping
row percentages or omit a new file simply because it is unassigned.

BUILDING.md describes current CTest/gcovr commands and per-group report generation.
The existing core CI report excludes GUI/PDF/V8 and is not a whole-product baseline.
The locally retained full Debug report under work/ recorded 1,527/1,953 lines and
1,542/3,414 compiler branches, but was produced on an uncommitted working tree;
at that time the worker and application entry point did not flush coverage data. A
subsequent direct worker protocol and application startup tests collect some
worker and app-entry coverage on normal exit; other paths remain unverified. Its raw branch
count includes compiler-generated edges and has not been classified. It is a
diagnostic snapshot, not a complete baseline or directly comparable to the older
binary-SDK and core-only figures in ../VERIFICATION.md. Reproduce against a recorded
revision/configuration, record exact toolchain and test set, collect all process data,
and produce per-group/component reports before claiming a comparable baseline.
Current reports and these known measurement gaps do not establish the adequacy of
any row. Additional critical paths and compiler edges need investigation; this is
an initial mapping, not the comprehensive adversarial audit.
