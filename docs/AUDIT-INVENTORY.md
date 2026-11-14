# Bootstrap audit coverage inventory

This inventory accounts for implemented sources and explicitly unfinished areas.
It is an initial audit map, not a closed nine-phase audit. Specifications derive
from PROJECT-ARCHITECTURE.md and the user's supplied catalogs/contracts, not from
existing tests. Arguments and assumptions: CORRECTNESS-ARGUMENTS.md. Executed
results and unexecuted configurations: ../VERIFICATION.md.

| Source/subsystem | Invariants and operations | Evidence/oracle | Open paths / status |
| --- | --- | --- | --- |
| src/document_state.cc | Reader exclusion, one-way mode, revision ordering | core tests; scalar transition argument | Checked overflow guard not reached by 2^64 mutations; owner-thread assumption |
| src/atomic_file.cc | Bounded read, complete write, old/new publication, cleanup | Independent payload bytes; fork crash, syscall fault injection, bounded-tmpfs ENOSPC, original comparison | Further syscall/allocation failure paths and physical durability assumptions remain open |
| src/autosave.cc | One committed target, forced/timed policy equivalence, dirty retry | core tests; invalid/missing/temp snapshot oracles | Full live-PDF serializer composition; interval preferences UI; validators are injected contracts |
| src/script_process.cc | Fixed bounded packets, ownership, deadline, kill/reap, request isolation | IPC tests with independently constructed malformed packets and fake worker | Worker-request malformed packets, syscall allocation/send/recv failures, signal interruption, FD inheritance audit |
| src/script_runtime.cc; src/script_worker.cc | Only worker links V8, no Node APIs, errors and timeout | Real V8 CTest execution and direct worker protocol tests | Direct protocol test allows normal exit and collects worker coverage; the parent still kills its worker before flushing. OS sandbox/total resource caps/persistent contexts unfinished |
| src/pdf_document.cc | MuPDF C exception boundary, page bounds, owned pixels, permissions, snapshot/copy | Real MuPDF tests, independent colored fixtures and encrypted profiles | Real binary-SDK tests cover encrypted user/owner credentials, permission classes, copy preservation and View/Print annotation flags; certification/lazy-object/fault cases remain open; no mutation tools |
| src/main_frame*.cc; src/app.cc | wx thread affinity, document owner, Reader initial/reopen, one-time toggle | Actual wx event actions, resize/fit, wrong/cancel/valid auth, reopen and session retention | Native UI tested; additional dialog accept/failure and asynchronous executor coverage remains |
| src/print_plan.cc | Bounded ordered one-based range parsing, repeated pages, explicit blanks, odd/even selection positions | Portable parser/model-oracle tests | Verify blank placement and parity across preview, layouts and output; sheet geometry covered separately |
| src/view_zoom.cc | Numeric and dynamic page/width/height fit | Independent ratios and actual wx resize/page actions | High-DPI/multimonitor/platform matrix remains |
| src/sheet_layout.cc | Physical geometry, bounded sheets, N-up, booklet, poster, copy/duplex order | Exact independent placements, booklet sequences, tiling boundary and invalid-option tests | Broader property/fuzz corpus and physical fold/device capability matrix |
| src/printing.cc | Native settings ownership, output permission, frozen job and submission | Real wx PostScript file-device output; cancelled native properties | Accepted native property roundtrips, spooler failures, physical hardware untested |
| src/print_dialog.cc; src/sheet_painter.cc | Automatic invalidation, shared geometry, bounded ROI pixels, preview lifetime | Actual controls and PNG pixel oracles, ROI/print flags, real device output size/count | Physical color/duplex fidelity, fault injection, capability changes remain |
| scripts/prepare_dependencies.py; cmake/ | Compatible static roots, pinned-commit/ABI receipts, local prefix, serialized preparation | Builder unit tests, full Debug build/test reusing an installed prefix with an empty work directory, and network-isolated Ubuntu full build/test | Fresh upstream build, interruption and install tests required |
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

The local instrumented dialog snapshot in work/stage0-dialog-branches.json reports
748 compiler branches, of which 451 were uncovered; gcov labels 268 of those
uncovered branches as exception-related. The labels do not establish reachability
or excuse failure-path tests. The other uncovered edges also require classification.
This observation is tied to that local build and is not a project-wide conclusion.

The print preview navigation recovery path keeps its plan invalid if updating
the native Print button throws after a redraw and its rollback both fail. A
local symbol-interposition experiment reached that path on the native wx build,
but did not intercept the virtual call in the Ubuntu Docker build. That
experiment was removed from the registered suite; it is not portable evidence.
The double-redraw failure and plan invalidation remain covered, but a portable
way to inject the subsequent button-update failure remains to be established.
The same limitation applies to `wxMenuBar::Append` failure injection: native
symbol interposition exercised all three menu rejection paths and cleanup, but
did not intercept the virtual call in Ubuntu Docker. Those exploratory cases
were removed; a portable way to verify these rejection paths remains open.

For the native pinned MuPDF archive used in `work/pdf-password-error-full/`,
`objdump -d --disassemble=fz_var_imp .deps/install/lib/libmupdf.a` shows that
`fz_var_imp` is a single return instruction. The C++ compiler nevertheless emits
exception-related edges at calls to the `fz_var` macro in `pdf_document.cc`.
That particular implementation cannot throw from `fz_var_imp`; this observation
does not classify other MuPDF edges or establish the behavior of a different
archive or toolchain. The raw edges remain in the reported coverage denominator.
The same native archive's `fz_pixmap_width`, `fz_pixmap_height`,
`fz_pixmap_stride`, `fz_pixmap_samples`, and `fz_device_rgb` disassemblies
contain only field access, simple register operations and control instructions,
with no call that
could raise a MuPDF or C++ exception. Their compiler-emitted unwind edges in
`pdf_document.cc` are therefore not reachable through these implementations
under valid-pointer and defined-behavior assumptions. This is evidence about
that archive, not about memory corruption, other versions, or their callers.

In the current bootstrap, `pdf_document::snapshot()` returns the stored loaded
bytes unchanged, and there is no PDF mutation API. Consequently, after `save()`
has verified that the source still equals those bytes, its changed-snapshot
publication path cannot be reached through supported sequential operations. The
path is retained for planned cataloged mutation: `acrobat-features.json` includes
PDF text editing, while `reader-features.json` includes forms and annotations.
It must be tested when applicable live mutation is implemented. This argument
assumes no undefined behavior or concurrent mutation; the catalogs bound the
intended feature scope and do not themselves establish implemented behavior.

Allocation-failure injection inside wxWidgets event registration exposed a leak
in wx 3.2's `wxEvtHandler::Bind`/`DoBind` ownership path: a newly allocated
event functor, and at some failure points its table entry, remain allocated when
registration throws. The local 34-position sweep and LeakSanitizer evidence are
in `work/viewer-bind-leak-sweep-*.log`; normal GTK/fontconfig allocations also
produce a separate baseline leak report. Vellum's viewer and print dialog now use
static event tables instead of dynamic `Bind`, avoiding this project-owned use of
the affected ownership path. Event behavior and construction-time reentrancy are
covered by `ui.viewer_static_events`; the full native and Ubuntu suites passed.
The upstream wxWidgets allocation-failure defect itself remains unresolved and
other library-owned uses require separate assessment. The local LeakSanitizer
check found no `DoBind`/event-functor allocation stack after this change, while
retaining the separate GTK/fontconfig baseline leak report.

In the local GCC 16 `-O0 --coverage` build, `src/app.cc` reached 14/14 lines and
31/36 branches after normal, failed-open, allocation, constructor, error-reporting
and foreign-exception startup tests. Four of the five remaining branches occur in
the compiler's expansions of the throwing `new` expressions for `main_frame` and
`vellum_app`: in each, one test checks a literal zero after a successful constructor,
and the other checks a cleanup flag set to one immediately before that constructor.
The latter's taken cleanup path was exercised by injecting a constructor exception
and checking that its allocated storage is released. The opposite outcomes require
changing those fixed values or violating C++ allocation semantics; they are not
reachable through a conforming application input or failure. The fifth missing edge
is an exception from wx's UTF-8 converter accessor while processing an initial path;
its reachability and the external library's initialization contract need further
investigation. Thus this build's 95%-branch goal cannot be met by conforming tests
of this file without changing its instrumentation; do not exclude or falsify edges.
These findings do not establish startup correctness or apply automatically to
other compiler versions, flags or source files.

An exploratory one-shot allocation failure during print-dialog construction
terminated the process inside wxWidgets 3.2's `wxSpinCtrlDouble::SetDigits` /
`wxString::DoFormatWchar` path, even with an initialized parent window. The local
probe and debugger traces are retained under work/stage0-ui-alloc-*; the failure
occurs before Vellum can catch the exception. This is a dependency-boundary
resource-exhaustion limitation requiring investigation and a durable mitigation;
it is not evidence that all GUI allocation failures behave the same way. An
earlier probe without a parent also exposed a wxWidgets locale cleanup double
free, but did not model the normal initialized-window precondition.

In a separate fault-injection experiment, throwing immediately before
`wxComboBox::Create` initialized the viewer toolbar's native control caused
wxWidgets 3.2's partially constructed combo-box destructor to assert that its
GTK widget was missing; the headless test timed out in that assertion dialog.
This deliberately injected boundary failure does not establish that a real
allocation failure reaches the same state. Investigate reachability and a
dependency-level mitigation. The registered toolbar test injects after native
creation and therefore does not verify this earlier failure point.

An allocation failure in wx 3.2 string conversion while `wxTextCtrl` initializes
the print range leaks a four-byte conversion buffer inside wxWidgets before the
exception reaches Vellum. The recovery test selects the allocation with
`VELLUM_PRINT_ALLOC_SCOPE=wx-string` and verifies that Vellum cleans up and can
retry. `work/print-wx-string-stack.log` contains the native LeakSanitizer trace;
the separate fontconfig suppression was used for that check. The buffer is not
exposed to Vellum, so application cleanup cannot release it. This upstream
dependency defect requires a corrected wxWidgets implementation; functional
recovery must not be represented as a leak-free result. The focused functional
test disables leak detection under ASan for this known defect; the separate
print-construction leak test retains leak detection for application allocations.
To reproduce the dependency leak using an address-sanitized build, run the
following from the repository root, substituting its build directory:

```sh
LSAN_OPTIONS="suppressions=$PWD/tests/lsan-fontconfig.supp" \
ASAN_OPTIONS=detect_leaks=1 VELLUM_PRINT_ALLOC_SCOPE=wx-string \
python scripts/run_headless_test.py <asan-build>/vellum_print_ui_owned_allocation_tests \
    <asan-build>/reference.pdf 0
```

An injected C++ allocation failure inside V8's `Isolate::New` after its internal
isolate allocation, but before it returns a handle to Vellum, caused LeakSanitizer
to report 67,608 bytes retained by V8 in the local pinned build. The worker exits
and the OS reclaims its address space, but Vellum cannot dispose an isolate it
never received. This is an unresolved dependency failure-path limitation, not a
claim of exception-safe V8 initialization. The normal fault suite exercises this
case; the AddressSanitizer configuration explicitly skips this one case while
keeping leak detection for the others. The failing evidence is in the ignored
work/stage0-worker-81-asan-tests.log. Revisit with the pinned V8's allocation and
exception contract and any future system-ICU configuration.
