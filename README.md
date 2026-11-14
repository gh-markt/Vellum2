# Vellum 0.34

A GPLv3 C++23 PDF reader/editor foundation using wxWidgets >=3.2.6, MuPDF and
an isolated embedded V8 worker. Reader functionality comes first.

This source bootstrap opens PDFs, navigates pages and provides numeric zoom,
Fit page, Fit width and Fit height. Fit modes track the current page and viewport.
The custom Print dialog implements ranges (for example `1-3, 7-10 12-19`), odd/even
filtering, scaling, N-up, booklet, poster tiling, duplex/manual duplex, copies,
collation and automatically updated sheet preview. Preview and native output use
the same sheet plan and renderer. Printer/properties selection uses wxWidgets;
there is no direct CUPS dependency. Advanced production printing and full Acrobat
parity remain governed development work, not completed features.

```sh
cmake -S . -B work/build -DCMAKE_BUILD_TYPE=Debug
cmake --build work/build -j2
ctest --test-dir work/build -j "$(nproc)" --output-on-failure
./work/build/vellum
```

wxWidgets development files are required at configuration. The builder fetches,
builds and installs missing compatible static MuPDF/V8 dependencies locally.
No third-party sources or binaries are bundled. See docs/BUILDING.md for Slackware,
explicit preparation, offline reuse, coverage, sanitizers, Docker and installation.
A dependency-independent build is available with -DVELLUM_CORE_ONLY=ON.
MuPDF combined-distribution obligations are described in docs/LICENSING.md.

Use File/Open (or a command-line PDF path), the page selector/navigation buttons,
and the zoom combo or View menu. File/Print opens the detailed dialog; choose a
printer/properties, review the updated preview, then print. Options on its left
scroll vertically. Preview sides can be navigated and exported as PNG.

Documents may require an opening password. Reader mode never autosaves. Enabling
Editor mode is one-way until close/reopen and makes dirty state eligible for timed
full-snapshot autosave (default 60 seconds). Clean documents produce no snapshot.
A prohibited action prompts for the owner password; cancellation or a wrong
password changes no permissions or mode. Successful owner authorization lasts
only until document close. Print or Save authorization does not enable Editor.

Form-filling permission is separate from general editing. Permitted form changes
must be manually saveable in Reader mode without enabling autosave. Standard PDF
content-extraction restrictions do not prohibit byte-for-byte Save As; copies
retain encryption and permissions. This bootstrap displays existing forms but
has no interactive form/content/layer mutation tools yet. Live-model serialization
and mutation-specific save checks must precede adding them; current snapshots
preserve original loaded bytes. Recovery uses only the last committed snapshot,
ignores temporary files and never overwrites the original through autosave.

Start with AGENTS.md, AGENT-CONTRACT.md, CODING-STANDARDS.md,
PROJECT-ARCHITECTURE.md and IMPLEMENTATION-REVIEW.md. Continue substantial stages
in docs/DEVELOPMENT-STAGES.md, track catalog acceptance in docs/feature-status.json
and delivered work in CHANGELOG.md. Supplied catalogs remain immutable.

VERIFICATION.md records actual checks and their limits. Document JavaScript does
not autoexecute; process isolation is not yet a complete hostile-input sandbox.
