# Bootstrap verification — 0.34

Recorded 2026-11-12 UTC. This is verification of the source bootstrap, not closure
of the comprehensive product audit or the complete Acrobat feature catalogs.
Source and fixture hashes are in docs/verification-manifest.json.

## Executed configurations

- Six dependency-independent CTest suites passed in Debug, AddressSanitizer and
  UndefinedBehaviorSanitizer builds: viewer.zoom_sheet_layout,
  printing.portable_page_selection, core.autosave_recovery,
  ipc.process_shared_memory, catalogs.immutable and builder.unit.
- Five real-library executables passed in Debug, AddressSanitizer,
  UndefinedBehaviorSanitizer and Release (-O3) compatibility builds: test_pdf,
  test_permissions, test_ui, test_print_ui and test_print_device. These compile
  the production MuPDF/wx sources, not substitute engine/UI mocks.
- Actual UI events under Xvfb exercise navigation, manual zoom, all fit modes,
  mixed-page dimensions, viewport resize, large zoom/scroll regions, one-way Editor
  mode, reopening, opening-password failures/cancel, conditional owner prompts,
  failed/cancelled authentication and authorization retention across operations.
- Print controls automatically invalidate/recompose the preview. Tests cover N-up,
  booklet padding/binding, poster tiling, invalid options/ranges, grayscale and PNG
  pixel oracles. A real wx PostScript file device produces four expected sides and
  colored image data; a size regression guards against excessive raster upsampling.
  This is device-file output, not proof of physical printing or spooler completion.
- Real MuPDF tests cover page geometry/colors, encrypted copies, user versus owner
  passwords, form-save rights without general edit permission, extraction versus
  whole-file copy, strict recovery opening, low/high-quality print gates, and
  distinct View/Print annotation flags in whole-page and region rendering.
- Coverage-instrumented core and native builds passed; measured line/branch data
  and per-file counts are retained in the manifest. Core production coverage was
  457/496 lines (92.1%) and 488/754 branches (64.7%). Native SDK coverage was
  1289/1708 lines (75.5%) and 1274/3097 branches (41.1%). These overlap and must
  not be added; neither measures the unexecuted V8 runtime or app entry point. Reachable uncovered code is
  remaining verification work, not waived by passing tests.
- Formatting, Python syntax, immutable catalog checks, ZIP integrity and clean
  extraction/core build are packaging checks. Prepare-only configuration requires
  no dependency download. All project C++ checks use C++23, -Wall and -Werror.

Toolchain: GCC 14.2.0, CMake 4.4.4, Python 3.12.14, clang-format 23.1.5 and gcovr 8.8.
The compatibility SDK consists of unpacked Debian binary development packages:
wxWidgets 3.2.8, MuPDF 1.25.1 and Xvfb. It is outside the source archive; no MuPDF
or V8 source repository was cloned. This is not the pinned MuPDF 1.26.1 static-build
acceptance. Standard CMake full-build test targets reproduce the same test sources
against the builder's installed dependencies. For the SDK checks, a local CMake
harness compiled the listed production sources into a static facade and linked
these existing binary libraries; run_gui_tests.sh started Xvfb and the five tests
in one invocation. Logs stay in ignored work directories and are not shipped.

AddressSanitizer used ASAN_OPTIONS=detect_leaks=0 because default LeakSanitizer
fails under the execution environment's ptrace/tracing restriction. Leak checking
is not claimed verified. Upstream binary libraries were not sanitizer-instrumented.
Release test passage is not a product latency benchmark.

## Evidence scope and open obligations

Core storage tests exercise Reader dirty/forced-trigger suppression, timed and
forced policy equivalence, unchanged skipping, serialization/write retries,
captured-revision bookkeeping, atomic replacement, invalid/missing snapshots,
temporary-file exclusion, original preservation, bounded/Unicode reads and
separate-process interruption immediately before and after rename. Their tiny
independent validator is a storage-policy oracle, not a PDF parser.

IPC tests use an explicit fake worker for malformed packets, bounded shared-memory
ownership, status errors, timeouts, worker exit and fresh requests after failure.
They do not establish V8 behavior. Builder unit tests check receipts and ABI/machine
mismatches; they do not validate upstream compilation.

No pinned static MuPDF/V8 preparation, real V8 execution, Docker run, physical
printer/spooler acceptance, full security fuzzing or product latency benchmark was
executed. The user requested that dependency fetching remain the builder's job;
Docker is unavailable here. Those configurations and test targets remain included
and mandatory for product acceptance. Stage 0 is not complete.

Additional open coverage includes allocation/syscall/partial-write/EINTR/fsync and
spool failures, accepted native property roundtrips, certification/signatures,
large hostile PDF corpora and full lazy-object validation. No PDF mutation tools
are shipped: live-model serialization, mutation-class permissions and tests of
partially/fully filled forms surviving save/reopen must precede their addition.
The form-save permission policy is tested; interactive filling is not implemented.
OS sandboxing/resource caps must precede document-script autoexecution. Complete
catalog acceptance, independent audit and proof/assumption reconciliation remain
governed by IMPLEMENTATION-REVIEW.md. No universal or machine-checked correctness
claim is made.

## Subsequent local Stage 0 observations

On a later uncommitted working tree (not the packaged source revision covered by
the manifest), local native Debug and Ubuntu offline Debug/Release builds using
an already prepared static prefix each ran all 12 configured tests; the latter
two used Xvfb and disabled container networking. The native Xvfb coverage run also
passed 12 tests. The installed Debug UI opened the generated reference PDF and the
installed worker passed the V8 test; the native Release build passed three focused
tests and opened that PDF. Six Ubuntu core tests passed with `docker run --init`;
without it, xvfb-run hung at startup on this host. Logs and configurations are in
ignored work/stage0-*. The exact source state of these observations was not captured
as a clean revision: do not present them as verification of a later source revision.

The local coverage snapshot reports 1,527/1,953 lines and 1,542/3,414 compiler
branches. It excludes or misses subprocess data as described in
docs/SUBSYSTEM-INVENTORY.md, is not comparable to the earlier SDK/core figures,
and does not establish full subsystem coverage. In that run the worker was killed
before normal coverage flush and no clean application exit was measured. Fresh
Ubuntu dependency preparation, offline-independent source builds, Ubuntu application
Release smoke, broad installation acceptance, native printer properties/failure
paths and coverage/inventory reconciliation remain open. Existing uncommitted
dependency and UI changes require review; see the working tree and Stage 0 plan.

Since that snapshot, direct real-worker protocol and application startup tests
have exercised normal exits and collected coverage data. They do not measure every
worker or entry-point path; the observed production parent killed its worker after
each request. These historical reports and their configuration limitations remain
in ignored work/stage0-* logs; they do not establish coverage for later revisions.

Fresh Ubuntu preparation subsequently built the pinned MuPDF and V8 from source
after adding the missing `unzip` prerequisite. Offline Debug and Release builds
against that fresh static prefix each passed all 17 tests under Xvfb with a private
1 MiB tmpfs for ENOSPC and `ctest -j "$(nproc)"`; Release includes the real app
startup test. The Ubuntu container image was
`sha256:9b8ed76ac0d82b2d4cbbc23ecce5c38d35a0c379c63903bf8dc23e2d6e92595e`,
with GCC 15.2.0, CMake 4.2.3 and Python 3.14.4; upstream V8 uses its own fetched
toolchain. Details are in ignored work/stage0-ubuntu-fresh-{resume,full,release}.log.
The uncommitted test inputs are captured by work/stage0-fresh-evidence.patch and
work/stage0-fresh-evidence-base.txt; their SHA-256 digests are respectively
`6201ba718d5a060302ff582f4c9f5e4869982dbd084876e73b5c704d46370955` and
`3248edee1552ae166dd532be4dee5e22695da614c6b86f7ba4bfc6122809ebad`.
The patch includes documentation changes made after the tests; it must be applied
to the recorded base to reproduce the source/test inputs. These checks do not
establish full stage acceptance, physical spooler behavior, complete coverage or
all remaining native-device and platform acceptance.

Fresh preparation of the pinned static dependencies also succeeded on Slackware
15.0+, followed by a native Debug build and the headless nproc-parallel suite:
16 tests passed and the bounded disk-full case was skipped in the normal run.
That case passed separately with the native test binary on a private 1 MiB tmpfs
in a container. Logs are in ignored work/stage0-native-{fresh,final}* files.
The native print-device test uses a virtual output and controlled dialog hooks;
it does not establish physical printer-driver behavior.

Subsequent headless testing added viewer zoom/navigation and authorization cases,
printer-default and preview-failure cases, IPC size/timeout boundaries, and a
print-layout validation matrix. The cumulative local Vellum-owned source report
in ignored work/stage0-verification-final-{lines,branches}.txt shows 1,742/1,975
lines and 1,882/3,456 compiler-reported branches. It uses the existing cumulative
instrumented build and excludes upstream V8 and MuPDF code. The nproc suite passed
16 tests with the bounded disk-full case skipped; that test passed separately on a
private 1 MiB tmpfs. Layout boundaries also passed UBSan and ASan with leak
detection disabled. These results do not
establish coverage of physical printer drivers, accepted native file dialogs,
or the other untested failure and interaction paths.
