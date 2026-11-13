# Native release validation — 2026-11-13

## Box checks

Ubuntu 24.04.5, 2 vCPU, 4 GiB RAM, GCC 13.3, system Qt 6.4.2. Builds used C++20, two parallel Ninja jobs, dynamic Qt linking, and no LTO. No physical mouse or exposed GPU was available on Box.

- Release core and native window suites passed. Covered protocol masks, assignments, left-click ordering, rollback and failed rollback, selective restore, private storage, profile persistence/rebase, mock macro lifecycle, Save versus Apply, preset switching, response preservation with Off, restore, and 2D picking.
- AddressSanitizer and UndefinedBehaviorSanitizer core/window suites passed. Leak detection was disabled for Qt/process-global allocations.
- **312 protocol oracle cases** matched retained Python merge bytes, decoded JSON, revision hashes, and invalid-input acceptance/rejection.
- **67 native macro sequences and preset files** parsed/loaded in upstream Input Remapper 2.2.0, revision `0d7f824eebae06acdf02a1e2fd4b8855a2573847`. No events were injected. Integration mock covered normal start/stop and validation; physical trigger/injection is not part of this release check.
- Private D-Bus test verified native KDE discovery, property writes, readback, stale-revision rejection, and rollback after a simulated property error. No working desktop was changed by this test.
- Private offscreen GUI checks captured all tabs and 3D preview, saved a macro, and restarted with a saved non-default preset.
- Archive and Debian packages built with generated library dependencies. Archive installer and menu entry were verified on Box.

Static mock GUI idle sample: **25,719 KiB PSS (~25.1 MiB)**, 43,752 KiB RSS, and **0.0% of one CPU core** during a five-second window. This measures a static Qt offscreen GUI, not animated previews or real hardware polling. An earlier active-animation sample used about 4.8% of one core. These are Box measurements, not a comparison benchmark against Electron or a guarantee for every PC.

## Local GM320 checks

- Native binary ran against the workstation's newer Qt runtime.
- Discovery identified the configuration interface only: USB `30fa:1440`, interface `01`, descriptor SHA-256 `0ec3eb2dc293bd8edcb490f5cee9ab272ebec8f4e0edc5f9f365f1bab53b2018`. Other interfaces and keyboard endpoints were not eligible.
- Initial native read matched the full pre-port settings revision.
- Native polling transaction **1000 → 500 → 1000** passed write/readback and selective restore; the full original revision and all other decoded fields were restored. This validates stored polling fields, not measured physical frequency.
- Both mock and hardware-connected GUIs rendered on a separate headless KWin Wayland screen with private runtime, display socket, configuration, data, and session bus. No input was automated on the user's desktop.
- KDE pointer/scroll read succeeded for the matching physical pointer. Production desktop settings were not changed.

Private snapshots, original settings, HID dumps, authentication, and personal presets are excluded from this repository and release artifacts.

## Remaining validation and scope

Full physical behavior for every button, DPI stage, timing value, lighting mode/palette, software macro trigger, and GNOME/Cinnamon/X11 adapter remains to be measured. Full vendor RGB parity, onboard macros/profiles/keyboard programming, and additional hardware drivers are not implemented. Hardware safety depends on exact matching, preservation of unmodified bits, stable reads, revisions, private backups, bounded writes, and verified rollback; those checks are maintained even for experimental features.
