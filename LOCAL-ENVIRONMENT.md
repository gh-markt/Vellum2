# Local environment

- Use `~/venv/bin/python` for local Python work. The user has authorized installing
  needed modules into this virtual environment. Python 3.12.14, gcovr 8.8 and
  coverage.py 7.17.3 are available there; invoke C++ coverage with `~/venv/bin/gcovr`.
- This checkout has wxWidgets (`wx-config`) and Docker installed. Availability of
  a command does not establish that a particular build or test has passed.
  During the Stage 0 build, Docker's default network could not reach Ubuntu package
  repositories; `docker build --network host` reached them.
  Run the test image with `docker run --init`: without an init process the image's
  `xvfb-run` waited indefinitely at startup here; with it the core suite passed.
- The local environment has a display, but assume test environments are headless.
  Run GUI tests with a virtual display such as Xvfb; do not depend on a physical
  display. `Xvfb` and `xauth` are
  installed locally, but `xvfb-run` is not available on the host.
  The XTest runtime `libXtst.so.6` is available for native file-dialog tests.
- Run tests with `ctest -j "$(nproc)"`. Disk-intensive cases and GUI cases sharing
  a virtual display use resource locks. Disk-full tests require a separate bounded tmpfs; never fill a
  shared or host filesystem.
- Local dependency installations are under `.deps/install`, and dependency source
  and build trees are under `work/dependencies`. Keep logs under ignored `work/`.
- The `cmake-build-debug` cache uses `VELLUM_DEPENDENCY_JOBS=auto`. At the time
  of configuration, this host exposed 24 CPUs and about 67 GiB available memory;
  the automatic count is recalculated when each dependency needs a build.
- The host reports ICU 79.1 through `pkg-config`; the Ubuntu test image reports
  ICU 78.2. The local default V8 archive is built against the host ICU; the previous
  bundled-ICU archive and receipt are preserved under `work/v8-bundled-backup-l85cv2c3`.
  Build a separate V8 archive for the Ubuntu environment; do not reuse the host one.
