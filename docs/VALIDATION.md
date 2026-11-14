# Orbit 0.4.0 validation — 2026-11-14

## Orbit branding and GM320 model

- The visible app/menu name is Orbit, with an original orbital mark. `orbit-mouse` and the existing compatibility command start the same binary. Package identifiers, private settings paths, preset version and the single-instance lock remain compatible with previous native releases.
- The bundled GLB was reproducibly generated on Box from the pinned official top image and the reviewed side/oblique GM320 listing views. It has **15,735 vertices and 26,252 triangles**, eight control materials/anchors, the asymmetric thumb rest, side fire switch, two thumb switches, grooved wheel, center controls, RGB diffusers, cable strain relief, and the original printed product markings. Final asset SHA-256: `2e680a7fe705ab2e5660a70b4eb75ac43023ccb1ef9357316e0b40636a21f2be`.
- Release and ASan/UBSan core/window suites passed after the final model changes. Model checks cover top-control order and picking, all three left-side controls, opposite-side occlusion, hidden underside controls, off-image coordinates, 2× scale picking and fire-editor selection without any mock write. Existing transaction and compact-control tests remain in the same suites.
- All **312 protocol oracle cases** passed. The private KDE D-Bus discovery/write/readback/stale/rollback check passed with zero production desktop writes. No protocol or macro backend behavior changed in this release.
- Isolated Box GUI renders covered all five pages, original photo, perspective/top/left/right/front/underside model views, assignment and fire editors, default/compact viewports and saved-preset restart. The immutable mesh loads on demand once; rendered frames are cached and hidden/static previews stop animation.
- Static Box GUI sample after the final 0.4 model changes: **43,794 KiB PSS (~42.8 MiB)**, 52,384 KiB RSS, and **0.0% of one CPU core** over five seconds. A separate 30-frame model run at 1440×900 physical pixels measured **16.443855 ms median / 21.686249 ms p95** per frame. These measure CPU software rendering on Box; continuous optional rotation consumes CPU and is off by default.
- Hardware-connected local Wayland verification passed on a separate 1366×768 headless KWin screen with private display socket, runtime, session bus and XDG directories. It captured the original image and new model/camera presets along with existing pages. No test input reached the working desktop. The before/after device read retained the entire current result, revision and settings, with **zero hardware writes or input injection**. The production saved-preset files were preserved.
- The reconstruction follows the product's visible contours and details. Unverified physical dimensions, inferred height/underside and manufacturing tolerances are documented in [model provenance](../assets/GM320-MODEL.md); this is not measured CAD or a scan. Fire-button remapping is still disabled, with only seven verified assignment registers.

## Earlier native-release validation

The following records cover releases 0.1–0.3.

### Box checks

Ubuntu 24.04.5, 2 vCPU, 4 GiB RAM, GCC 13.3, system Qt 6.4.2. Builds used C++20, two parallel Ninja jobs, dynamic Qt linking, and no LTO. No physical mouse or exposed GPU was available on Box.

- Release core and native window suites passed. Covered protocol masks, assignments, left-click ordering, rollback and failed rollback, selective restore, private storage, profile persistence/rebase, mock macro lifecycle, Save versus Apply, preset switching, response preservation with Off, restore, and 2D picking.
- AddressSanitizer and UndefinedBehaviorSanitizer core/window suites passed. Leak detection was disabled for Qt/process-global allocations.
- **312 protocol oracle cases** matched retained Python merge bytes, decoded JSON, revision hashes, and invalid-input acceptance/rejection.
- **67 native macro sequences and preset files** parsed/loaded in upstream Input Remapper 2.2.0, revision `0d7f824eebae06acdf02a1e2fd4b8855a2573847`. No events were injected. Integration mock covered normal start/stop and validation; physical trigger/injection is not part of this release check.
- Private D-Bus test verified native KDE discovery, property writes, readback, stale-revision rejection, and rollback after a simulated property error. No working desktop was changed by this test.
- Private offscreen GUI checks captured all five pages, assignment/fire editors, the lighting palette and the 3D preview, saved a macro, and restarted with a saved non-default preset. Captures covered both 1280×820 and 1040×720 windows.
- The 0.3.0 Razer-layout / Logitech-palette redesign passed release and sanitizer checks for mouse/callout picking, all navigation pages, staged button editing and Discard, independent DPI-stage editing and nearest supported-value selection, brightness-code preservation, and keyboard fire-button selection without hardware writes or an invented eighth setting. A layout check verified the complete compact assignment panel, DPI/rate/timing controls, lighting caption/live speed and macro preview/enable/stop controls lie inside their page viewport at 1040×720; Customize and Macros require no page scrolling at that size. The fixed footer also fits at the 1040×680 minimum.
- The official GM320 image and lighting derivatives were prepared on Box from a pinned source hash. Eight target regions and eight callout selections passed the picking tests, including the visible side fire button. Screenshots verified transparent compositing and alignment at default and compact sizes.
- Archive and Debian packages built with generated library dependencies. Archive installer and menu entry were verified on Box.

Static packaged 0.3.0 mock GUI idle sample with the official image: **35,077 KiB PSS (~34.3 MiB)**, 52,336 KiB RSS, and **0.0% of one CPU core** during a five-second window. This measures a static Qt offscreen GUI, not animated previews or real hardware polling. The 0.2.0 sample was 30,129 KiB PSS and the 0.1.0 sample was 25,719 KiB PSS; an earlier active-animation sample used about 4.8% of one core. These are Box measurements, not a comparison benchmark against Electron or a guarantee for every PC.

### Local GM320 checks

- Native binary ran against the workstation's newer Qt runtime.
- Discovery identified the configuration interface only: USB `30fa:1440`, interface `01`, descriptor SHA-256 `0ec3eb2dc293bd8edcb490f5cee9ab272ebec8f4e0edc5f9f365f1bab53b2018`. Other interfaces and keyboard endpoints were not eligible.
- Initial native read matched the full pre-port settings revision.
- During the 0.1.0 validation, native polling transaction **1000 → 500 → 1000** passed write/readback and selective restore; the full original revision and all other decoded fields were restored. This validates stored polling fields, not measured physical frequency.
- Both mock and hardware-connected GUIs rendered on a separate headless KWin Wayland screen with private runtime, display socket, configuration, data, and session bus. No input was automated on the user's desktop.
- KDE pointer/scroll read succeeded for the matching physical pointer. Production desktop settings were not changed.
- The installed 0.3.0 package passed a hardware-connected native Wayland check on a private 1366×768 screen, using the official GM320 image. The preferred window was reduced to fit the available height, and the footer remained visible. Captures covered all five pages, assignment/fire editors, the lighting palette, saved macro sequence, optional 3D schematic and compact views. The verification made no hardware writes or input injection; a before/after read retained the current device revision and every decoded setting. The user's 0.2.0 binary was kept locally for rollback. The open 0.2.0 process was not interrupted. Personal snapshots and presets were not uploaded.

Private snapshots, original settings, HID dumps, authentication, and personal presets are excluded from this repository and release artifacts.

## Remaining validation and scope

The eighth physical control is the small fire button beside left click, confirmed by the user. Its separate remapping command remains unverified; only seven assignment registers are implemented. Full physical behavior for every button, DPI stage, timing value, lighting mode/palette, software macro trigger, and GNOME/Cinnamon/X11 adapter remains to be measured. Full vendor RGB parity, onboard macros/profiles/keyboard programming, and additional hardware drivers are not implemented. Hardware safety depends on exact matching, preservation of unmodified bits, stable reads, revisions, private backups, bounded writes, and verified rollback; those checks are maintained even for experimental features.
