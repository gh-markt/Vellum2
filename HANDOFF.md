# Current recovery checkpoint

Version 0.34 corrected bootstrap is packaged and verified as recorded in
VERIFICATION.md. Native binary-SDK Debug/Release/ASan/UBSan and six core suites
pass; dynamic fit/scrolling, detailed live print preview, native PostScript output,
open/user/owner password rules and Reader form-save permission are covered.

Next builder action: Stage 0 in docs/DEVELOPMENT-STAGES.md. Build pinned static
MuPDF/V8 using the preparation target on a wxWidgets development system; run full
CTest, Ubuntu Docker and remaining native printer/property/failure acceptance.
No dependency source repositories were cloned here. Do not infer V8/static-build
or physical-spooler acceptance from the binary SDK compatibility runs.

Before implementing forms/layers/content changes, add coherent live serialization
and mutation-specific permission checks, then verify manual Reader saves without
autosaves and dirty Editor snapshot recovery. Current snapshots preserve original
bytes because mutation tools are not yet present. Detailed remaining gaps and
coverage are in VERIFICATION.md and docs/AUDIT-INVENTORY.md.
