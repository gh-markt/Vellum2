# Building Vellum

## Linux prerequisites

Install GCC/Clang with C++23 support, CMake >=3.25, GNU make, Python >=3.10, Git,
pkg-config and wxWidgets >=3.2.6 development files (wxGTK), including its printing
framework. Vellum does not require CUPS headers or a direct CUPS library. V8 preparation also needs the upstream depot_tools prerequisites,
network access to Chromium repositories/CIPD, Ninja, a suitable Clang/sysroot and
substantial free disk/RAM. Budget tens of GiB for V8, start with two build jobs.
No third-party sources or binaries are shipped. No OS package manager is invoked
by Vellum's builder. Slackware-current is supported as a native target; use its
wx-config and compiler. wxWidgets 3.2.6 is the compatibility baseline.

## Full build (dependencies prepared automatically at build time)

```sh
cmake -S . -B work/build -DCMAKE_BUILD_TYPE=Debug
cmake --build work/build -j2
ctest --test-dir work/build --output-on-failure
cmake --install work/build --prefix "$HOME/.local"
```

Normal configuration requires wxWidgets immediately. The build's dependency
stage fetches/builds only missing compatible MuPDF/V8 static installations into
.deps/install; source checkouts and intermediate builds go into work/dependencies.
Use VELLUM_DEPENDENCY_JOBS to bound dependency compilation independently of -j.
V8 is linked only into the installed libexec/vellum/vellum-script-worker executable.
The UI does not yet expose script execution; tests invoke the worker by exact path.

## Prepare dependencies separately

```sh
cmake -S . -B work/prepare -DVELLUM_PREPARE_ONLY=ON
cmake --build work/prepare --target prepare
```

This configuration does not require wxWidgets because it builds no UI. The prepare
target is idempotent using version/ABI receipts, serializes access to the prefix,
and resumes existing upstream working directories. The receipts are written only
after libraries, headers and licenses have been installed. Interrupted installation
without a valid receipt is rebuilt. Logs can be captured under work/.

## Existing static installations and offline reuse

Pass -DVELLUM_MUPDF_ROOT=/absolute/prefix and/or -DVELLUM_V8_ROOT=/absolute/prefix.
The layout must match .deps/install: lib/libmupdf.a, lib/libmupdf-third.a,
include/mupdf, lib/libv8_monolith.a and include/v8 as applicable. Each dependency
requires share/vellum/mupdf.json or v8.json with version and machine matching
scripts/prepare_dependencies.py; V8 also requires exact gn_args from the lock file.
Receipts assert a compatible build; do not manufacture one for an unknown binary.
Build known-compatible external installations using the same script and pass its
prefix. Reusing both valid installations needs no downloads. Arbitrary distro
V8/node binaries are not silently substituted for a static embedded V8 build.

The lock file pins MuPDF and V8 by exact release commits. depot_tools is resolved
from its tool branch and its exact commit recorded in the receipt. This is not
fully hermetic toolchain reproducibility; pin depot_tools/toolchain artifacts as
part of release engineering. Check upstream support/security and compatibility
before shipping; bootstrap pins are not a statement of current security support.

## Dependency-independent verification

```sh
cmake -S . -B work/core -DVELLUM_CORE_ONLY=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build work/core -j2
ctest --test-dir work/core --output-on-failure
```

This builds no GUI/PDF/V8/native-printing integration. Do not call it a full product build.

## Coverage and sanitizers

```sh
cmake -S . -B work/coverage -DVELLUM_CORE_ONLY=ON -DVELLUM_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build work/coverage -j2
ctest --test-dir work/coverage --output-on-failure
gcovr --root . --filter src/ --exclude-directory '.*vellum_fake_worker[.]dir.*' --object-directory work/coverage --txt --txt-metric branch work/coverage
cmake -S . -B work/asan -DVELLUM_CORE_ONLY=ON -DVELLUM_SANITIZER=address -DCMAKE_BUILD_TYPE=Debug
cmake --build work/asan -j2
ctest --test-dir work/asan --output-on-failure
```

Repeat in a separate directory with VELLUM_SANITIZER=undefined. TSan applies when
concurrent project state is introduced; do not interpret process IPC as proof of
future thread safety. Full builds also accept these flags, but instrumentation of
Vellum does not instrument upstream libraries. Store reports under work/coverage.

## Docker and formatting

```sh
docker build -t vellum-ci -f Dockerfile .
docker run --rm vellum-ci
find include src tests -type f \( -name '*.cc' -o -name '*.hh' \) -exec clang-format --dry-run --Werror {} +
```

Docker defaults to core verification; use --build-arg VELLUM_CORE_ONLY=OFF to
prepare static dependencies and compile full integration targets. CI runs both
core coverage and the full Ubuntu Docker build. Benchmarks must use a separate
Release build with -O3, never sanitizer/debug timings. See VERIFICATION.md for
actual checks performed when this source bootstrap was packaged.
