# Changelog

Newest releases belong at the front. Add delivered features to the active release
section and reference catalog IDs when their acceptance criteria are actually met.
Do not claim whole catalog features from partial infrastructure.

## 0.34 — Reboot bootstrap

- Recheck dependency receipts and installed headers on each build, repairing
  incomplete or incompatible managed installations without manual stamp removal.
- Separate test registration and common allocation interception while retaining
  the individual fault scenarios and assertions.
- Clean up atomic-write temporary files if allocation fails during publication.
- Remove a newly created private recovery directory if constructing its path fails.
- Preserve the current viewer session if preparing a replacement document title fails.
- Clean up failed Editor-mode recovery setup and hide stale page images after render errors.
- Keep the prior zoom policy if preparing a new zoom label fails.
- Commit owner credentials without a fallible allocation after granting permissions.
- Reject printer measurements with an empty printable area.
- Reject malformed poster sides before attempting to render their labels.
- Exit the isolated script worker cleanly if startup throws an exception.
- Complete script-worker cleanup before reporting a successful result.
- Report a disconnected script worker promptly instead of waiting for a timeout.
- Preserve operating-system errors when waiting for a JavaScript worker fails;
  test IPC resource, spawn, communication and interruption failures in batches.
- Test atomic persistence under interrupted/short writes, synchronization and
  rename failures, and actual disk exhaustion on a dedicated bounded tmpfs.
- Exercise application startup and clean exit, adversarial PDF render bounds,
  and print selection, permissions and virtual-device error paths under Xvfb.
- Exercise real V8 worker success, script errors and malformed requests through
  its protocol, including normal exit for coverage collection.
- Reuse installed static dependency archives without requiring the source build
  trees, and reject dependency receipts for commits other than the pinned ones.
- Remove the direct CUPS dependency from sources, CMake, CI and prerequisites;
  use the wxWidgets native printer-settings adapter instead.
- Add a portable static print-plan foundation with ordered range parsing,
  repeated pages, explicit blank pages, selection-position odd/even filtering and
  adversarial/model-oracle tests.
- Establish the full portable printing specification, including scaling, poster
  tiling, booklet/duplex/N-up and automatic preview sharing the final sheet plan.
  Implement the custom wx dialog, shared preview/output painter and native submission.
- Add dynamic numeric/fit-page/fit-width/fit-height zoom and bounded viewport rendering.
- Add N-up grids/orders, booklet padding/binding, poster overlap/marks/labels,
  actual/fit/shrink/custom scaling, copies/collation and manual/automatic duplex.
- Add open-password handling and conditional owner-password prompts; retain owner
  authority per session without forcing Editor mode. Wrong/cancelled attempts
  preserve state. Distinguish extraction rights from encrypted whole-file copies.
- Add actual MuPDF/wx UI, print-preview and PostScript-device regression tests.

- Establish C++23 CMake targets for separate document, persistence, recovery,
  PDF, JavaScript IPC, printing and wxWidgets UI static subsystems.
- Add a local static MuPDF/V8 preparation stage, explicit dependency roots,
  dependency-free core verification, installation rules and Ubuntu CI/Docker recipes.
- Add document-scoped irreversible Reader-to-Editor mode and revision-based
  autosave policy, with Reader-mode changes retained across the transition.
- Add full-snapshot storage with same-directory temporary files, atomic rename,
  synchronization, one retained committed snapshot and no recovery replay.
- Add a deterministic autosave trigger using the production commit path, plus
  process-interruption and failure regression tests.
- Add source adapters for MuPDF rendering/copy, wxWidgets printer settings and a V8 worker
  using bounded Unix control IPC and shared memory.
- Preserve both supplied feature catalogs and the autonomous development contract;
  establish architecture, coding standards, adversarial audit instructions,
  feature tracking and substantial ordered development stages.

Integration verification and catalog-feature completion are recorded separately
in VERIFICATION.md and docs/feature-status.json; source presence is not parity.

0 0.33 Complete overahaul
