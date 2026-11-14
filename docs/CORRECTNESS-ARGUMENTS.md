# Bootstrap correctness arguments and assumptions

These are reviewed source-level arguments, not machine-checked formal proofs or
an assertion that the full product has been audited. Tests challenge their
implementation correspondence; coverage gaps are listed in VERIFICATION.md.

## Reader exclusion and revision tracking

Specification: Reader mode never autosaves; prior Reader changes become eligible
on enabling Editor mode; a successful snapshot clears only its captured revision.

Model: (mode, current, committed), initially (Reader,0,0), with nonnegative
64-bit revisions and committed <= current. changed increments current after
checking overflow. enable_editor changes Reader to Editor and is otherwise a no-op.
autosaved(r) accepts committed <= r <= current and sets committed to r. No public
operation transitions Editor to Reader; creating a new session creates new state.

Base case satisfies committed <= current. changed preserves it because increment
is checked and committed is unchanged. The mode transition does not alter either
revision. autosaved preserves it by its precondition checks. These transitions
therefore preserve the ordering for every finite sequential execution of the model.

Both timer and forced operations reach trigger_now. Before calling the serializer
or store, it checks needs_autosave, which is exactly Editor && current != committed.
Thus a Reader trigger cannot serialize or publish a snapshot. Exceptions before
return from store.commit leave committed unchanged. After return, autosaved uses
the revision captured before serialization, so later changes are not cleared.

Assumptions/correspondence: document_state and autosave_controller are used on a
single document owner thread; serializers represent a coherent captured revision;
external code does not call autosaved to falsely certify persistence. These APIs
are not thread-safe. Test coverage includes Reader mutations/forced triggers,
one-way transition, unchanged skipping, timer boundary, failed serialization/write,
invalid revision order and an edit occurring during serialization. A test sets the
revision to one below the maximum using a test-only member pointer, then checks the
last increment, overflow rejection and unchanged state, and commitment at the limit.
Linux fault injection also checks that failure while constructing either rejection
exception leaves state unchanged. The boundary setup does not simulate 2^64 edits.
An independent bounded model additionally explores every six-operation sequence
from edit, enable-Editor and autosave-at-zero/current/future choices, including
rejected transitions; it compares mode, revision and dirty status after each
operation. This finite exploration does not replace the induction or exercise
concurrency.

## Snapshot publication and recovery

The `write_all` loop maintains this invariant: the temporary file contains the
initial prefix of the requested byte sequence, and the remaining view is exactly
the suffix after that prefix. Initially the prefix is empty. Assuming a successful
POSIX write reports a positive count no greater than the requested length and
writes precisely that prefix, removing the reported count preserves the invariant.
An EINTR result leaves the view unchanged; a zero or other failed write throws.
By induction, every successful finite sequence of positive writes leaves the full
requested byte sequence in the temporary file when the view becomes empty. If a
write fails after partial progress, publication is not attempted and cleanup
attempts to unlink the temporary pathname. After the temporary descriptor is
created, its already-allocated name is used directly for rename and cleanup, so
no subsequent pathname allocation can fail before cleanup is established. These
are source-level arguments about bytes and control flow, not mechanically checked
proofs of the OS or filesystem.

The argument assumes POSIX write semantics, no concurrent mutation of the private
temporary file, and successful resource operations until the stated failure point.
It does not prove termination under an endless sequence of EINTR results, physical
durability, successful cleanup if unlink itself fails, or correctness if an OS
violates its write contract. Tests exercise
fragmented writes with repeated EINTR, partial progress followed by ENOSPC, and
each observed C++ allocation failure in atomic publication, including the
unchanged previously committed target and absence of temporary files. Those
examples challenge the argument but do not establish its inductive step.

Private recovery-directory creation likewise removes the newly created empty
directory if constructing its returned path throws; this relies on `rmdir`
succeeding and does not claim recovery from a failure of the cleanup syscall.
Its observed allocation points are exercised separately.

Before rename, the old pathname has not been changed; the new bytes exist only
at a unique temporary pathname. A successful rename changes the final pathname
atomically from old to new under the filesystem assumptions below. Cleanup never
removes the final pathname. Consequently, an interruption around rename presents
the old or new complete file rather than a partial bytewise overwrite. A failure
following rename may still have published the new snapshot: the controller must
not claim durability, but retry remains safe because it replaces the same single
snapshot. File fsync precedes publication; directory fsync follows it.

Recovery opens only the exact committed .pdf target. It does not enumerate or
promote temporary candidates and never falls back after validation failure. Only
one committed target is written; old versions are replaced. The UI creates a
private session directory distinct from the original's location. Generic atomic
file APIs cannot themselves prove that a caller did not pass the original target;
that obligation belongs to session integration and its tests.

Assumptions: local POSIX filesystem with atomic same-directory rename; fsync is
honored; no hostile concurrent mutation of the private directory; snapshot
validation establishes the recoverability needed by supported operations. The
MuPDF adapter has real-library strict-open tests; full lazy-object validation
remains open. Fork/_Exit crash tests establish process-interruption
behavior, not physical power-loss guarantees. The test validator's tiny payload
format is a storage-policy oracle, never evidence of PDF validity.

## IPC bounds and lifetime

Control packets have a fixed size, magic/version and length <= mapping capacity.
The fixed memfd size is sealed against grow/shrink. A valid response length therefore
bounds the parent's read to the existing mapping. MSG_TRUNC detects packets larger
than the control buffer rather than accepting their prefix. Invalid status and
protocol fields are rejected before copying output. Each call owns distinct
sockets, mapping and child, so no accepted response is shared between requests.

A child_process object is constructed only after successful posix_spawn. Its
noncopyable destructor kills and waits for the exact live child before descriptor
and mapping destruction. Failure before spawn owns no child. The deadline loop
recomputes remaining time after EINTR rather than restarting the entire timeout.
Other poll errors preserve the operating-system error instead of reporting a timeout.
The worker copies source bytes before writing its result; the parent reads only
after a control response. Request/response sequencing assigns one payload writer
at a time. This is a protocol argument for cooperative peers, not a claim that an
arbitrary hostile worker cannot race its shared output. Even malformed-peer output
lengths remain bounded; output semantics and UTF-8 validation need higher-level
schemas before document host APIs are introduced.

Tests include UTF-8, empty and maximum payloads, oversize input, six malformed
response forms, script error status, missing executable, worker exit and timeout,
and a fresh successful invocation after failures. The test worker does not
establish V8 execution correctness. Tests of the embedded V8 worker separately
cover evaluation, compilation/execution errors, absence of `process` and
`require` globals, and an infinite loop deadline. Vellum does not use Node.js.
OS sandboxing and total resource caps remain hardening tasks, not established
properties of process separation.


## Portable page selection

Ranges are intervals of one-based physical document pages, expanded in expression
order. Each interval emits its pages in ascending order; repeated intervals emit
repeated pages. A run of n consecutive comma separators emits n-1 blank entries,
represented internally by zero. The independent oracle in test_print_plan builds
expected ordered sequences rather than using production parsing. Parity applies to
document page numbers, not imposed sheet positions; both parity filters omit blanks.

For an accepted expression, take the parser's ordered sequence of range tokens
and comma runs as the model. The outer-loop invariant is that `pages` is the
specified output for all completely consumed tokens, and `ranges` is the
unconsumed suffix. Initially the output is empty; a leading comma run is then
handled by the same blank rule before the first range. At each range, an inner
induction over its inclusive endpoints establishes that the result appends each
eligible page exactly once in increasing order: the base prefix is empty; each
step considers the next integer and appends it exactly when the requested subset
permits it. The endpoint checks bound the step. The following comma-run loop
consumes each separator once; its blank loop starts at one and appends one zero
for each remaining separator only for the all-pages subset. Thus it emits exactly
n-1 zeros for n commas, or none for either parity subset. Whitespace contributes
no output. Appending these results preserves the outer invariant, including order
and repeated ranges. On successful termination the suffix is empty, so the result
is the concatenation specified for the entire accepted expression. Rejection or
allocation failure returns no partial vector to the caller.

This is a reviewed source-level induction over the implementation's accepted
grammar, not a mechanically checked parser proof. It assumes the specified
`from_chars`, integer, string-view and vector behavior and no concurrent mutation
of the supplied character storage. It does not establish that every conceivable
human-readable page-range notation is accepted, nor correctness of later sheet
imposition. The independent tests exhaustively enumerate three-page sequences,
comma runs of one through four, repeats and both parity filters; these validate
examples and challenge the argument, but do not replace either inductive step.

Each endpoint is checked with from_chars and restricted to 1..page_count, at most
1,000,000, so the inclusive integer loop cannot overflow. Each iteration consumes
expression characters or advances a bounded page number. Expansion stops with an
exception before more than 1,000,000 selected entries can be stored, including
blanks and repeated pages. With parity filtering, at least half the entries of any
interval of length greater than one are emitted; single-page filtered intervals
are bounded by the 65,536-byte expression limit. Thus traversal is bounded by the
expression and output limits even for overlapping ranges. These bounds do not prove
arbitrary-size document support. Allocation failure remains an exception; the API
has no document/printer state to partially mutate.
Booklet transforms, tiling, native accept/cancel semantics and live preview are
not established by this argument and remain separate audit obligations.

## Authorization and document modes

The UI queries the relevant operation permission before requesting credentials.
Allowed operations return a grant without a password prompt. For denied operations,
cancellation returns no grant; invalid credentials are checked on a separately
opened candidate and cannot modify the live document. Only a verified owner grant
is committed to the live session. Owner state resides in the document object,
not global preferences, and is discarded on document replacement. Authentication
alone does not modify application mode; only the Editor operation performs that
transition. Tests challenge user-versus-owner, cancel, wrong credentials, allowed
operations, denied operations, cross-operation retention and reopening.

The owner password is copied before authenticating the live MuPDF document; after
successful authentication, the prepared string is swapped into recovery credentials
without allocation before the owner flag is set. Thus a C++ allocation failure
before live authentication cannot grant owner rights, and no credential-string
allocation remains between authentication and committing the flag. This assumes
MuPDF's authentication call either reports its result or follows its documented
error behavior, and does not establish behavior under arbitrary library corruption.
Allocation-failure tests use an owner password longer than the small-string buffer
and verify both permission state and subsequent recovery validation.

These arguments assume a single UI owner and MuPDF's standard authentication
semantics. They do not establish certification/signature policy. Save As preserves
the original security bytes; extraction permission is not treated as a file-copy
ban. Mutation-specific saving remains an obligation before adding mutation tools.

## Shared print geometry and bounded rasterization

The pure planner produces explicit paper-coordinate sides and clipped placements.
Preview and native output call the same painter with that plan. Native submission
freezes its own copy; controls are disabled during submission. Copies are imposed
once in the plan and backend copies are one. Thus the application has no separate
preview/output ordering algorithm. This establishes design correspondence, not
physical printer fidelity or a proof of every layout formula.

Each placement intersects its destination with its clip. The painter divides this
intersection into bounded raster tiles and maps each tile back to page coordinates.
Clockwise placements invert their transform before rendering and reapply rotation
afterward. Print usage honors PDF annotation/optional-content print intent. Device
transforms scale bounded images to paper rather than software-upscaling to device
pixels. Tests independently check geometry, colored preview pixels, print flags,
actual PostScript side counts and a regression limit on excessive output size.
Resource policies reject nonfinite, out-of-page and excessive raster requests.
The viewer uses the View usage and renders only its viewport; its annotation-flag
ROI regression is distinct from print tests. Allocation/driver/fault coverage is
still incomplete and explicitly tracked.

For a successfully composed manual-duplex job with all sides selected, the
current planner always supplies at least one back side. The nonempty selection
precondition gives regular layout at least one side; regular layout appends a
blank back when its initial side count is odd. Booklet layout pads a nonempty
selection to a positive multiple of four and emits a front and back per sheet.
Poster layout with manual duplex is rejected. With all sides selected, copying
retains every generated side and the validated copy count is positive; later
partitioning or reversing changes order but not membership. Thus erasing fronts
from such a successful job cannot leave an empty back list. This argument assumes
the current planner and a completed composition without an exception; it says
nothing about printer behavior or concurrent mutation. The dialog retains its
empty-back guard as a defensive boundary if future planners relax these rules;
its false outcome is not currently reachable through a valid composed job.

For a normally constructed, live `printer_settings`, `selected()` reads one
boolean and `preferences()` copies a structure of booleans, integers and an enum
from its owned implementation. Neither operation allocates, calls external code
or explicitly throws. Similarly, `print_dialog::valid()` reads one boolean and
`job()` returns a reference to an existing member. Under those object-lifetime
and ordinary C++ execution assumptions, these accessors cannot throw. Their
declarations do not promise `noexcept`, so a non-optimizing compiler can still
emit exception-cleanup edges around calls to them. This argument concerns only
these accessor bodies: it does not establish that callers, other printer methods,
or arbitrary future implementations cannot fail. Revisit it if the accessors or
their ownership invariants change.

The four clamping expressions in `sheet_painter::paint_tile` use built-in
floating-point arithmetic and compare `double` values against the fixed bounds
zero and one. For finite inputs, ordinary C++ floating-point execution, and the
standard `std::clamp` implementation, evaluating these expressions makes no
allocation or user-defined call that can throw a C++ exception. The bounds are
ordered, so the clamp's bound precondition holds. Thus an exception from one of
these expressions is not a reachable ordinary C++ failure path, even if a
non-optimizing build emits cleanup edges around the calls. This argument does not
prove the coordinates are finite, the resulting raster is correct, or execution
under floating-point traps, undefined behavior, corrupted state, or a modified
standard library. Check those separate assumptions and revisit the argument if
the expression or comparator changes; do not discard other uncovered branches
merely because these calls cannot normally throw.

The subsequent tile copy traverses the `std::vector<unsigned char>` returned by
the renderer and writes bytes to the successful `wxImage` buffer. With valid
iterator and buffer ranges, the current byte assignment and iterator operations
neither allocate nor invoke user code, so this copy cannot throw an ordinary C++
exception. This argument does not establish that the renderer supplied a buffer
of the right size or that wxWidgets allocated enough destination storage: those
are independent safety obligations, and a bounds violation would invalidate the
argument rather than make it prove safe behavior. Instrumented builds may still
show exception edges because the standard algorithm has no general `noexcept`
contract for arbitrary iterator types.

For the manual-duplex front/back partition, the `erase_if` predicates read the
`back` boolean of a live `print_side`. The current `print_side` consists of
scalar fields and a `std::vector` of placements using the standard allocator;
its moves, destruction and allocator deallocation do not throw under normal C++
execution. Consequently these specific erasures do not throw, although the
generic standard algorithm may lack a `noexcept` declaration and produce
compiler exception edges. This does not apply to the preceding job copies,
which allocate: the submission tests inject failures into each observed copy
allocation and check window restoration and retry. The argument assumes valid
object lifetimes, the current types and allocator, and no undefined behavior;
revisit it if those assumptions change.
