# Portable printing — governing requirements

This document extends PROJECT-ARCHITECTURE.md. The product target is the full
included Acrobat printing and print-production catalog, not a lowest-common-
denominator system dialog. Preserve catalog exclusions. These are implementation
requirements, not claims that the bootstrap already implements the finished dialog.

## Dependency and subsystem boundaries

Vellum has NO direct dependency on CUPS: no CUPS headers, discovery calls, linker
requirements, mandatory CUPS development packages or CUPS-specific application
logic. wxWidgets provides the platform printing interface and device selection/
properties UI. wxGTK or the OS may internally use CUPS; Vellum does not require
users to configure or build against a separate CUPS API. OS printing availability
and driver capabilities still determine what a device can physically do.

Separate portable print planning (vellum_print_plan, no wx/MuPDF/platform types),
PDF page rendering/composition through the engine adapter, wxWidgets dialog and
preview views, and wxWidgets device submission (vellum_printing). Split further
static libraries as these responsibilities grow. Store printer-native objects
inside adapter implementations. Keep engine-native objects out of the print plan.
A native dialog is a device/settings integration point, not the entire Vellum UI.

The bootstrap implements a custom wx dialog, pure sheet composition, live
preview and native job submission. Accepted native settings are retained; cancel
leaves them unchanged. All/current/typed page selection, parity, copies/collation,
reverse, actual/fit/shrink/custom scaling, rotation, margins, N-up grids/orders,
booklet left/right binding/padding, poster overlap/marks/labels, grayscale,
front/back subsets and manual duplex are implemented. Booklet selects landscape
and short-edge duplex by default. Poster requires simplex. The driver receives
one application-composed side at a time with backend copies set to one.

Changing a control immediately invalidates Print and schedules a 100 ms preview
rebuild. Invalid options keep submission disabled. Preview side navigation and
PNG export use the same sheet painter as native output. Print output uses bounded
visible-region raster tiles at physical output DPI and device transforms rather
than allocating or upsampling full pages. The plan is capped at 10,000 sides.
PNG export encodes before atomically publishing the image: failures before rename
preserve an existing destination, while a failure after rename can leave the new
image visible and must still be reported.
Native properties feed paper bounds, orientation, duplex, copies/collation and
color back into the custom controls and preview. Backend-specific tray/finishing
semantics still require platform acceptance. Manual duplex explicitly submits
fronts, asks for reinsertion and permits cancelling before backs.

This is still partial coverage of the complete catalog: selected-area output,
page-label mapping, presets, comment summaries/form-only output, automatic media
selection and advanced production/color/preflight controls are not implemented.
The requirement table below includes these future obligations. Tests exercise a
real wx PostScript file device; physical printers/spoolers remain unverified here.

## Required user-facing behavior

| Capability | Required behavior |
| --- | --- |
| Selection | All pages, current page, selected pages and selected area/current view where supported by catalog; typed disjoint ranges such as `1-3, 7-10 12-19`; meaningful errors |
| Parity and order | All, odd-only or even-only positions within the selected sequence; reverse order; copies and collation with an explicit order of operations |
| Size | Actual size, fit, shrink oversized pages, custom percentage; auto-rotate and center; orientation; page-box selection and paper selection by PDF page size where supported |
| Multiple per sheet | Configurable grid, page order, orientation, margins/gutters and borders; single application of layout with driver N-up disabled |
| Booklet | Imposed front/back sheet pairs, blank padding, left/right binding, full/front-only/back-only printing and short-edge duplex; fold/order preview |
| Poster/tiling | Scale across sheets, overlap, cut marks, tile labels and page/tile selection; correct clipping and placement at printable margins; no missing strips at tile boundaries |
| Device | Printer selection/properties, paper/media/tray/size, printable bounds, resolution, color/monochrome/grayscale, supported simplex/duplex binding modes, copies and collation |
| Content | Document, comments/markups, form-only output and applicable combinations; comment summaries; annotation/widget print flags and optional-content print intent |
| Raster fallback | Print as image at chosen bounded resolution with predictable memory usage; preserve page geometry and physical size |
| Presets/output | Save and restore print presets, validate them against changed device capabilities; PostScript/file output where included and supported |
| Preview | Automatically refresh the actual imposed sheet/side preview after relevant setting changes; show sheet/side/page mappings, printable bounds, scale, orientation, tiling overlap and blank padding |
| Production | Included ICC/color-management, separations, overprint/ink warnings, rendering intent, spot-color mapping, transparency, marks/bleed/trim and other print-production catalog requirements; dedicated adapters/dependencies as justified |

The portable selector uses one-based physical document page numbers in the typed
order, including repetitions; reverse/copies are subsequent planning operations.
Two consecutive commas insert one blank page, three insert two, and so on; whitespace
between commas does not change this. Blanks can appear at the beginning or end of
the expression. A single leading or trailing comma is invalid. Odd/even applies to
one-based positions in the complete selected sequence, including explicit blanks,
before reverse, copies or imposition; document page numbers do not determine parity.
Whitespace and commas separate ranges; whitespace around a hyphen is accepted.
Reject numeric zero, negatives, overflow, descending ranges, out-of-document pages
and missing endpoints. An empty
filtered result is valid selection output but must disable job submission with an
explanation. Page-label selection requires an explicit later mapping/UI distinction;
never silently interpret Roman labels as physical indices.

Booklet mode forces short-edge duplex and all selected pages, disabling the duplex
and parity controls until another layout is selected. Once a printer is selected,
paper selection belongs to its properties; a nominal paper choice is available only
for preview before selecting a printer. Native printer measurements govern output.
Odd-only or even-only selection forces one-sided printing and disables the duplex
control. Selecting all pages or a custom range clears the parity filter and makes
duplex available again, except while booklet mode requires short-edge duplex.
N-up also fixes the parity filter to all pages and disables that control; duplex
remains available for N-up. Both layouts allow custom ranges; all pages here means
all entries in the chosen range, including explicit blanks.
The custom range starts empty and is cleared when switching to any non-custom
page selection; returning to custom selection requires a new range.
Media choices and defaults are output-device specific: PDF output may offer both
document-page sizing and fixed sheet sizes, including for N-up and tiling, while a
physical device may support only certain media. Do not claim per-page PDF-sized
output until the backend can actually produce it.

Bootstrap parser limits are 1,000,000 document pages, 65,536 expression bytes and
1,000,000 selected entries, including repeats and blanks. The output limit bounds
expansion of repeated or overlapping ranges; reject excess rather than truncating.
Limits are resource policy, not feature-parity claims.

## One plan for preview and print

An immutable job specification captures the owning document/session generation,
document revision, selected pages, composition settings and accepted device
capabilities. A pure planner converts it into ordered sheets, sides, placements,
clipping rectangles and transforms, using explicit physical units. State the
transformation order, page boxes, rotations, scaling, overlap, printable margins,
duplex flips and copy/collation order. Include the plan format/version in presets
where needed; never serialize backend pointers or wx native handles.

Preview and actual output MUST consume the same sheet plan and geometry. Do not
maintain independent booklet/tiling algorithms or let both Vellum and the driver
apply N-up, scaling or copies. Preview rendering may use screen resolution and
print rendering device resolution, but source pages, placement, clipping, blank
padding and side order must agree. Device capabilities and color profiles can
limit physical color fidelity; show applicable soft-proof/profile status rather
than implying screen pixels guarantee exact physical ink colors.

Changing any relevant control validates settings, creates a new specification/
preview generation and schedules rendering automatically. Responsiveness may use
short debouncing, cancellation and caching; stale generations must never replace
new previews or enable printing against an older configuration. Show pending/error
states. Preview updates must not dirty the PDF or create an autosave. Document
close/replacement, edits while a dialog is open, printer changes and cancellation
must invalidate/rebind ownership explicitly before submission.

## Device integration and failure behavior

Use wxWidgets to select devices/settings and render/submit pages. Discover only
capabilities the selected backend actually exposes. Preserve native properties
when translating portable options, and detect unsupported combinations. Unsupported
hardware duplex must not be represented as enabled; offer explicit manual duplex
when appropriate. Never silently drop a requested setting or submit a different
plan from the one the user approved. Device-specific property changes must feed
back into validation, planning and preview when they affect geometry or behavior.

Manual duplex needs front/back subset planning, feed/reinsert instructions and
explicit continuation/cancel states. Handle printer selection cancel, unavailable
printers, unsupported media, permission restrictions, failed begin/render/end job,
partial submission, cancellation and changed capabilities. Distinguish local
acceptance from actual spool/device completion according to platform support.
Expose opaque job identifiers and capabilities, not CUPS types or assumed semantics.

## Automated acceptance and adversarial verification

Map AR-089 through AR-095 and AR-159 to AC-217 through AC-223 and their full-catalog
counterparts. Reconcile all included print-production IDs AC-224 onward; exclusions
remain immutable and must be stated rather than implemented as false promises.
This document supplements the catalogs and does not edit them.

Use independent mathematical oracles for page sets, physical transforms and sheet
ordering. Cover odd/even after disjoint-range selection, repeated and overlapping pages,
consecutive-comma blanks, their typed positions and layout interactions,
invalid syntax, huge repeated ranges and bounded memory/time. Booklet tests must
verify physical folding/order for odd and even document sizes, padded blanks,
binding edges and duplex flips. Tiling tests must establish coverage without gaps,
correct overlap/clipping/marks, and boundary behavior for page-size exact multiples,
fractional dimensions and tiny printable areas. Test scale/aspect/unit conversions,
mixed media/orientation, copy/collation order and content/print-flag policy.

Exercise actual wx controls and automatic preview invalidation under Xvfb/native
platform runners. Compare exported/rasterized output to the preview's exact plan,
not screenshots alone. Use a fake device backend to deterministically inject errors
and assert no unintended submission. Test native device/property accept/cancel
integration and a controlled virtual printer without physical printouts. Reconcile
portable, integration, UI, line/branch coverage, sanitizers and relevant fuzz/model
checks under IMPLEMENTATION-REVIEW.md. All print options need positive, negative,
boundary, adversarial and compositional tests before their features are complete.
