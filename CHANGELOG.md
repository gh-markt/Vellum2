# Changelog

Newest releases belong at the front. Add delivered features to the active release
section and reference catalog IDs when their acceptance criteria are actually met.
Do not claim whole catalog features from partial infrastructure.

## 0.34 — Reboot bootstrap

- Remove the direct CUPS dependency from sources, CMake, CI and prerequisites;
  use the wxWidgets native printer-settings adapter instead.
- Add a portable static print-plan foundation with disjoint range parsing,
  deduplication, document-page odd/even filtering and adversarial/model-oracle tests.
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
