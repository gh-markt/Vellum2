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
invalid revision order and an edit occurring during serialization. Revision overflow
is guarded but is not exercised by 2^64 edits in the bootstrap tests.

## Snapshot publication and recovery

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
The worker copies source bytes before writing its result; the parent reads only
after a control response. Request/response sequencing assigns one payload writer
at a time. This is a protocol argument for cooperative peers, not a claim that an
arbitrary hostile worker cannot race its shared output. Even malformed-peer output
lengths remain bounded; output semantics and UTF-8 validation need higher-level
schemas before document host APIs are introduced.

Tests include UTF-8, empty and maximum payloads, oversize input, six malformed
response forms, script error status, missing executable, worker exit and timeout,
and a fresh successful invocation after failures. The test worker does not
establish V8 execution correctness. Real V8 tests separately cover evaluation,
compilation/execution errors, missing Node APIs and an infinite loop deadline.
OS sandboxing and total resource caps remain hardening tasks, not established
properties of process separation.


## Portable page selection

Ranges are intervals of one-based physical document pages. The independent oracle
in test_print_plan uses direct page membership comparisons, not the difference
array used by production. Marking +1 at each interval start and -1 after its end
makes the prefix sum equal the count of intervals containing each page. Hence a
positive prefix sum selects exactly the union, and scanning page numbers ascending
makes output sorted and duplicate-free. Applying parity to that original page
number implements document-page odd/even semantics independent of interval order.

Each endpoint is parsed with checked from_chars and validated before indexing.
The one-million-page bound permits last+1 indexing within a page_count+2 vector.
The 65,536-byte input bound limits interval multiplicity to at most 32,768, so
prefix counts fit the 32-bit int used on the supported target platforms. Running
time is O(expression bytes + page count), rather than pages times ranges. The
limits do not establish full arbitrary-size document support. Allocation failure
remains an exception; the API has no document/printer state to partially mutate.
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
