# Building Vellum

## Linux prerequisites

Install GCC/Clang with C++23 support, CMake >=3.25, GNU make, Python >=3.10, Git, unzip,
pkg-config, system ICU >=73 development files (`icu-uc` and `icu-i18n`), and
wxWidgets >=3.2.6 development files (wxGTK), including its printing
framework. Vellum does not require CUPS headers or a direct CUPS library. V8 preparation also needs the upstream depot_tools prerequisites,
network access to Chromium repositories/CIPD, Ninja, a suitable Clang/sysroot and
substantial free disk/RAM. Budget tens of GiB for V8.
No third-party sources or binaries are shipped. No OS package manager is invoked
by Vellum's builder. Slackware-current is supported as a native target; use its
wx-config and compiler. wxWidgets 3.2.6 is the compatibility baseline.

## Full build (dependencies prepared automatically at build time)

```sh
cmake -S . -B work/build -DCMAKE_BUILD_TYPE=Debug
cmake --build work/build -j2
ctest --test-dir work/build -j "$(nproc)" --output-on-failure
cmake --install work/build --prefix "$HOME/.local"
```

Normal configuration requires wxWidgets immediately. Each build rechecks the
dependency receipts and required installed artifacts, then fetches/builds only
incompatible or incomplete MuPDF/V8 installations into .deps/install; source
checkouts and intermediate builds go into work/dependencies.
Dependency builds default to `VELLUM_DEPENDENCY_JOBS=auto`, independently of the
outer build's `-j`: each build uses available RAM after reserving the greater of
2 GiB or 20%, estimates 2 GiB per V8 job and 1 GiB per MuPDF job, and caps jobs
to `nproc`, CPU affinity and visible cgroup memory and CPU limits. If `nproc` is
unavailable, affinity and cgroup limits still apply. At least
one job runs even under severe memory pressure. These are planning estimates,
not peak-memory guarantees; other activity and compiler/linker memory can vary.
Set `-DVELLUM_DEPENDENCY_JOBS=<positive integer>` to override the memory-based
count; the explicit count is still capped at `nproc` and CPU affinity.
Existing CMake caches retain their previous setting; set it to `auto` explicitly
to adopt automatic sizing.
V8 is linked only into the installed libexec/vellum/vellum-script-worker executable.
The V8 archive uses Chromium's system-ICU shim; the worker links the environment's
ICU shared libraries and the compiler's atomic runtime. V8 is configured with `use_sysroot=true` and the current
environment's `/` as its target sysroot. The Docker image installs `libicu-dev`.
Native and Docker environments must build their own ICU-matched V8 archives.
The UI does not yet expose script execution; tests invoke the worker by exact path.
The headless native file-dialog workflow test requires the X11 XTest runtime
library (`libXtst.so.6`, packaged as `libxtst6` on Ubuntu) and an X server with
the XTest extension, such as Xvfb.

## Prepare dependencies separately

```sh
cmake -S . -B work/prepare -DVELLUM_PREPARE_ONLY=ON
cmake --build work/prepare --target prepare
```

This configuration does not require wxWidgets because it builds no UI. The prepare
target is idempotent using version/ABI receipts, serializes access to the prefix,
and resumes existing upstream working directories. It invalidates the old receipt
before repair and records the installed header manifest only after libraries,
headers and licenses have been installed. Missing headers, changed build options,
and interrupted installations trigger repair. MuPDF's old objects are cleaned
before rebuilding so changed options cannot silently reuse them. Logs can be
captured under work/.

## Existing static installations and offline reuse

Pass -DVELLUM_MUPDF_ROOT=/absolute/prefix and/or -DVELLUM_V8_ROOT=/absolute/prefix.
The layout must match .deps/install: lib/libmupdf.a, lib/libmupdf-third.a,
include/mupdf, lib/libv8_monolith.a and include/v8 as applicable. Each dependency
requires share/vellum/mupdf.json or v8.json with version, pinned commit, machine,
and installed-header manifest. MuPDF also records its make options and build output;
V8 requires the current system ICU version and
library directory, effective GN arguments, and backport digest in its receipt.
Older receipts without those fields are incompatible and are rebuilt automatically
for the managed prefix; explicitly supplied roots must be rebuilt by their owner.
An old bundled-ICU archive or one built against another environment's ICU is rejected.
The ICU 79 Unicode compatibility backport is documented in `patches/README.md`.
Receipts assert a compatible build; do not manufacture one for an unknown binary.
Build known-compatible external installations using the same script and pass its
prefix. Reusing both valid installations needs no downloads. Arbitrary distro
V8/node binaries are not silently substituted for a static embedded V8 build.

`docs/dependencies.lock.json` specifies the pinned commits, build options and
installed artifact paths used by the builder and CMake. depot_tools is resolved
from its tool branch and its exact commit recorded in the receipt. This is not
fully hermetic toolchain reproducibility; pin depot_tools/toolchain artifacts as
part of release engineering. Check upstream support/security and compatibility
before shipping; bootstrap pins are not a statement of current security support.

## Dependency-independent verification

```sh
cmake -S . -B work/core -DVELLUM_CORE_ONLY=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build work/core -j2
ctest --test-dir work/core -j "$(nproc)" --output-on-failure
```

This builds no GUI/PDF/V8/native-printing integration. Do not call it a full product build.

## Coverage and sanitizers

```sh
cmake -S . -B work/coverage -DVELLUM_CORE_ONLY=ON -DVELLUM_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build work/coverage -j2
ctest --test-dir work/coverage -j "$(nproc)" --output-on-failure
gcovr --root . --filter src/ --exclude-directory '.*vellum_fake_worker[.]dir.*' --object-directory work/coverage --txt --txt-metric branch work/coverage
cmake -S . -B work/asan -DVELLUM_CORE_ONLY=ON -DVELLUM_SANITIZER=address -DCMAKE_BUILD_TYPE=Debug
cmake --build work/asan -j2
ctest --test-dir work/asan -j "$(nproc)" --output-on-failure
```

Repeat in a separate directory with VELLUM_SANITIZER=undefined. TSan applies when
concurrent project state is introduced; do not interpret process IPC as proof of
future thread safety. Full builds also accept these flags, but instrumentation of
Vellum does not instrument upstream libraries. Store reports under work/coverage.

The `VELLUM_CORE_ONLY` example is a core-only report, not a project-wide baseline.
Tests use nproc parallelism; disk-intensive recovery tests use a CTest resource
lock. GUI tests each start a private Xvfb display, so they can run concurrently;
their runner forces GTK's X11 backend, uses local GIO and disables session-bus
auto-launch to avoid desktop services and portal mounts in temporary homes.
Allocation batches reserve four CTest processor slots
for their own workers.
The disk-full test uses an empty dedicated tmpfs mount no larger than 16 MiB. It
uses `VELLUM_DISK_FULL_TEST_DIR` when provided, or creates a private 1 MiB tmpfs
in a user and mount namespace. CI supplies the mount explicitly where namespace
creation may be restricted. Missing provisioning is a test failure, not a skip;
never point the test at a shared filesystem.
For a full report configure without that option, build with `VELLUM_COVERAGE=ON`,
and run the complete applicable CTest suite. GUI tests start Xvfb themselves.
The RAM-backed coverage command runs the instrumented tests first and then
measures their counters. `testing.coverage_reporter` tests the reporting tool's
handling of counters and errors; it is not the application coverage measurement.
CMake's `vellum_target`
instruments project C++ targets; CTest registers standalone C++ executables and
Python builder/catalog checks. Full configuration generates reference.pdf via
scripts/make_fixture.py; encrypted and print fixtures are in tests/fixtures/ with
provenance in its README. The fake worker exercises the IPC boundary, not V8.
Its test-only source is not coverage-instrumented because worker termination can
interrupt counter writes; linked project-owned transport code remains instrumented.
Some device tests can be skipped; record skips rather than counting them as passes.

For RAM-backed counters and reports, activate the native environment and run:

```sh
source ~/venv/bin/activate
python scripts/coverage_in_ram.py --build work/coverage-full
```

The command prints the temporary report directory and sampled peak filesystem
usage. It runs CTest with available processor parallelism, collects subprocess
counters on tmpfs, and copies gcov notes there before producing project reports.
Use `--persist work/coverage-reports` to copy reports to a new directory when
evidence must survive the RAM filesystem. `--regex` runs a subset and its report
must be identified as incomplete. The runner rejects insufficient free space and
known counter-write errors. Its size check is an estimate, so monitor measured
full-suite peak usage and increase Docker and CI tmpfs allocations with headroom
as the project grows. The initial 128 MiB CI allocation was chosen after a native
full-run peak of about 13.5 MiB; it is not a permanent upper bound. Compiler
instrumentation still adds execution overhead even when output is in memory.

Generate project and subsystem reports from the *same* full build, test run and
gcda data. For example, after a full instrumented disk-backed run in `work/coverage-full`:

```sh
mkdir -p work/coverage-reports
gcovr --root . --filter '(src|include)/' --object-directory work/coverage-full --txt-metric branch --txt work/coverage-reports/project-branches.txt work/coverage-full
gcovr --root . --filter '(src|include)/' --object-directory work/coverage-full --txt work/coverage-reports/project-lines.txt work/coverage-full
gcovr --root . --filter 'src/(print_plan|sheet_layout)\.cc$' --object-directory work/coverage-full --txt-metric branch --txt work/coverage-reports/planning-branches.txt work/coverage-full
gcovr --root . --filter 'src/(print_plan|sheet_layout)\.cc$' --object-directory work/coverage-full --txt work/coverage-reports/planning-lines.txt work/coverage-full
```

Use the primary source ownership map in SUBSYSTEM-INVENTORY.md to repeat the
filtered commands for each subsystem, including its relevant instrumented headers,
and individual source filters for important components. Shared semantic
responsibilities must be assessed separately without adding overlapping totals.
Retain the full unfiltered source report and identify unassigned sources, excluded
or uninstrumented code, missing subprocess data, skips and compiler-generated edges.
The project filters cover only Vellum-owned `src/` and `include/` paths; do not
include V8 or MuPDF source files in any project, subsystem or component denominator.
Vellum's own MuPDF adapter and V8 worker remain in scope.
Current CI produces aggregate core and full reports; automated generation of all
subsystem/component reports and complete worker/app collection remain work to do.
Record revision and working-tree state, build flags, compiler/gcov/gcovr versions,
fixtures, exact commands and test outcomes. Compare only equivalent configurations
and instrumentation. Coverage rules and interpretation are in
VERIFICATION-ARCHITECTURE.md; the reports are not pass/fail numerical gates.

## Source package

Run `cmake --build <build-directory> --target package` to create
`vellum-<project-version>.zip` in the build directory. The ZIP preserves repository
paths and includes the current working-tree contents of files in the Git index,
including staged additions and unstaged edits to tracked files. Untracked files are
excluded. Missing indexed files, unresolved merges, and submodules cause an error;
an existing archive is replaced only after a successful build.

## Docker and formatting

```sh
docker build -t vellum-ci -f Dockerfile .
docker run --rm --init --tmpfs /faultfs:rw,size=1m,mode=0700 \
  -e VELLUM_DISK_FULL_TEST_DIR=/faultfs vellum-ci
find include src tests -type f \( -name '*.cc' -o -name '*.hh' \) -exec clang-format --dry-run --Werror {} +
```

Docker defaults to core verification; use --build-arg VELLUM_CORE_ONLY=OFF to
prepare static dependencies and compile full integration targets. CI runs both
core coverage and the full Ubuntu Docker build. Benchmarks must use a separate
Release build with -O3, never sanitizer/debug timings. See VERIFICATION.md for
actual checks performed when this source bootstrap was packaged.
