# Bootstrap audit coverage inventory

This inventory accounts for implemented sources and explicitly unfinished areas.
It is an initial audit map, not a closed nine-phase audit. Specifications derive
from PROJECT-ARCHITECTURE.md and the user's supplied catalogs/contracts, not from
existing tests. Arguments and assumptions: CORRECTNESS-ARGUMENTS.md. Executed
results and unexecuted configurations: ../VERIFICATION.md.

| Source/subsystem | Invariants and operations | Evidence/oracle | Open paths / status |
| --- | --- | --- | --- |
| src/document_state.cc | Reader exclusion, one-way mode, revision ordering | core tests; scalar transition argument | Checked overflow guard not reached by 2^64 mutations; owner-thread assumption |
| src/atomic_file.cc | Bounded read, complete write, old/new publication, cleanup | Independent payload bytes; fork crash and exception injection; original comparison | OS allocation/partial-write/EINTR/fsync failure injection; physical durability assumptions |
| src/autosave.cc | One committed target, forced/timed policy equivalence, dirty retry | core tests; invalid/missing/temp snapshot oracles | Full live-PDF serializer composition; interval preferences UI; validators are injected contracts |
| src/script_process.cc | Fixed bounded packets, ownership, deadline, kill/reap, request isolation | IPC tests with independently constructed malformed packets and fake worker | Worker-request malformed packets, syscall allocation/send/recv failures, signal interruption, FD inheritance audit |
| src/script_runtime.cc; src/script_worker.cc | Only worker links V8, no Node APIs, errors and timeout | Real V8 CTest target supplied | Real V8 execution untested here; OS sandbox/total resource caps/persistent contexts unfinished |
| src/pdf_document.cc | MuPDF C exception boundary, page bounds, owned pixels, permissions, snapshot/copy | Real MuPDF tests, independent colored fixtures and encrypted profiles | Real binary-SDK tests cover encrypted user/owner credentials, permission classes, copy preservation and View/Print annotation flags; certification/lazy-object/fault cases remain open; no mutation tools |
| src/main_frame*.cc; src/app.cc | wx thread affinity, document owner, Reader initial/reopen, one-time toggle | Actual wx event actions, resize/fit, wrong/cancel/valid auth, reopen and session retention | Native UI tested; additional dialog accept/failure and asynchronous executor coverage remains |
| src/print_plan.cc | Bounded one-based range parsing, deduplication, odd/even document pages | Portable parser/model-oracle tests | Parser complete for documented bounded syntax; sheet geometry covered separately |
| src/view_zoom.cc | Numeric and dynamic page/width/height fit | Independent ratios and actual wx resize/page actions | High-DPI/multimonitor/platform matrix remains |
| src/sheet_layout.cc | Physical geometry, bounded sheets, N-up, booklet, poster, copy/duplex order | Exact independent placements, booklet sequences, tiling boundary and invalid-option tests | Broader property/fuzz corpus and physical fold/device capability matrix |
| src/printing.cc | Native settings ownership, output permission, frozen job and submission | Real wx PostScript file-device output; cancelled native properties | Accepted native property roundtrips, spooler failures, physical hardware untested |
| src/print_dialog.cc; src/sheet_painter.cc | Automatic invalidation, shared geometry, bounded ROI pixels, preview lifetime | Actual controls and PNG pixel oracles, ROI/print flags, real device output size/count | Physical color/duplex fidelity, fault injection, capability changes remain |
| scripts/prepare_dependencies.py; cmake/ | Compatible static roots, version/ABI receipts, local prefix, serialized preparation | Builder unit tests and prepare-only configure | Actual upstream build, reuse, interruption, offline reuse and install test required |
| catalogs and feature ledger | Immutable catalogs, IDs and scopes preserved | SHA-256 and JSON/ID checks | Feature acceptance/interaction oracles must be derived as features are implemented |

All production .cc files are listed. Headers define their public boundaries;
cmake/version.hh.in generates version.hh. Tests themselves require oracle review:
the fake worker and tiny snapshot validator test only the integration boundary
they claim, and cannot close MuPDF or V8 correctness. A generated simple PDF cannot
stand in for a broad hostile/real-world corpus. Coverage percentages count executed
branches, not proof assumptions or inter-subsystem correctness.

Editing, transactions/history, embedded-document ownership, signatures/crypto/trust,
advanced rendering, accessibility, production-print features, sandboxing and persistent scripting
are not implemented. Their exclusion from bootstrap execution is scope, not feature
completion; they remain in the immutable catalogs and DEVELOPMENT-STAGES.md.

Closure requires filling the open rows and reconciling them against the final
source tree and requirements, with positive, negative, boundary and adversarial
coverage. Do not silently classify a reachable failure path as out of scope.
