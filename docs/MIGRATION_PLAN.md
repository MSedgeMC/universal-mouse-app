# Native migration plan

Date: 2026-11-13. Repository: `MSedgeMC/universal-mouse-app`.

## Decision

Use **C++20, Qt 6 Widgets and CMake/Ninja**. C++ and Rust both offer efficient compiled runtimes; neither is inherently faster for this application. C++ is the faster implementation path here because Qt supplies the native GUI, accessibility, D-Bus, JSON, SVG/2D rendering and optional OpenGL facilities needed by the existing application.

The performance opportunity is removing Chromium/Electron and repeated Node/Go/Python bridges, keeping I/O asynchronous, and stopping work when views are idle. Preserve the protocol's deliberate transfer delays. Reducing them without measurements would change hardware behavior, rather than prove a faster port.

Use Qt's distro binaries and dynamic linking. Do not embed a browser or move the old HTML into a WebView. The finished native app should have no runtime dependency on Electron, Node, Go or application Python helpers. The separately installed Input Remapper service retains its own Python dependency.

## Scope and success criteria

The first release ports all *supported* functionality of the current GM320 application. It ships on Linux first. Core models and the UI should be portable; Windows/macOS support require their own hardware, macro and desktop integrations and validation.

The repository name describes an extensible driver architecture. Initial supported identity is the captured Instant `30fa:1440` configuration interface `01` with the pinned feature descriptor. GM320 and the upstream X16v2 identity must be reported with their actual validation level. Matching only a vendor ID or generic mouse name is insufficient. Unknown devices get diagnostics and no configuration writes.

| Area | Native parity requirement | Current evidence/limit |
| --- | --- | --- |
| Discovery | Device identity, interface, descriptor and capabilities; reconnect/error states | Exact GM320 descriptor and stable reads established locally |
| Buttons | Seven stored assignments, Disabled, action validation, reaction-bit preservation, last-left-click guard | Stored slots do not establish every physical button; most writes unmeasured |
| DPI/polling | Six bounded DPI stages, enabled/active stage, captured rates | Polling write/readback/rollback established; physical frequency and persistence unmeasured |
| Lighting | Captured effects, palettes, participation, brightness, speed, direction, symmetry and button response where supported | Keep unavailable effects unavailable; preserve read-only unrecognized values |
| Device timing | Retain device timing read/edit rules and existing-value preservation | It is not the Linux double-click threshold; GM320 effect unvalidated |
| Local presets | Four profiles; Save stages locally, Apply changes hardware; Undo, refresh and unsaved-exit behavior | These are local presets, not hardware profile banks |
| Linux macros | 67 keys, down/up/delay timeline, 64 events, balanced releases, 30-second limit, emitted Back/Forward/Middle triggers | External Input Remapper; physical trigger execution untested |
| Macro lifecycle | Confirm injector RUNNING, restore after interrupted starts, confirm stop, wait for pending operations on exit | Crash/forced-kill recovery remains explicitly documented |
| KDE settings | Per-mouse pointer acceleration and native scroll factor through KWin | Persisted desktop settings are independent of the app's local presets |
| Other desktops | GNOME/Cinnamon pointer behavior and existing X11 wheel integration where supported | GNOME/Cinnamon currently global; imwheel is external/X11 only |
| Viewer | Clickable 2D mouse, optional 3D rotation/zoom/selection, button/scroll testing within the app, lighting/timeline previews | Generic geometry; no bundled reference-photo gallery |

Full official-vendor parity, onboard macro upload, onboard keyboard programming, hardware bank switching and additional unsupported RGB modes are **later protocol research**, not silently added during the language port.

## Architecture

```text
src/
  core/                 typed settings, capabilities, validation, revisions, presets
  drivers/
    driver_registry.*   compiled driver registration and matching
    instant/            GM320/X16v2 decoder, encoder, transactions, captured fields
  platform/
    linux/              hidraw, KWin, Input Remapper, desktop/X11 integrations
  ui/                   Qt Widgets window, tabs, view models, dialogs, mouse viewer
  cli/                  headless discovery/read/mock commands for diagnostics/tests
tests/
  fixtures/             reviewed descriptor/configuration/transaction fixtures
  unit/                 model, encoder/decoder, transaction, failure recovery
  integration/          mock transport and private D-Bus services
  gui/                  isolated native GUI verification
packaging/              desktop entry, icon, udev rule, dependency/license metadata
tools/                  Box build commands and isolated-screen verification
legacy/resa/            reference implementation, unchanged until parity is proven
```

- Start with a compiled-in driver registry rather than arbitrary plugin loading. Drivers expose validated capabilities, typed reads/changes and transactional Apply/Restore operations.
- Use a dedicated worker thread for device transactions. The GUI receives progress/results through Qt signals and remains responsive. Serialize writes; a stale UI revision must trigger reload instead of overwriting newer mouse settings.
- Use a small reviewed Linux hidraw adapter preserving feature report ID/length, opened-device metadata checks, descriptor hashes, private locking and constrained commands. Isolate the necessary low-level operations behind a typed transport interface.
- Use Qt DBus for KWin and Input Remapper. Retain fixed Input Remapper CLI calls where device grouping/preset handling requires upstream behavior; remove the application's Python bridge helpers. Preserve exact identifiers, including trailing spaces.
- Use atomic private JSON files with schema versions for presets/backups. Provide explicit export/import of the legacy profile JSON through the old app; never scrape Electron LevelDB or automatically upload personal settings.
- Use ordinary Qt widgets with Ubuntu Medium text, keyboard navigation, focus and accessible names. QPainter/SVG supplies the 2D view; QOpenGLWidget supplies an optional 3D view with a 2D/software fallback. Redraw on changes and active animation, not continuously on static tabs.
- Keep diagnostics available without showing raw report implementation details in normal product flows.

## Milestones

### 0. Public source baseline and reproducible workspace

Publish this plan, MIT attribution, clean legacy source, captured fixtures and support limits. Keep credentials, current presets/device backups, screenshots/logs, vendor binaries and raw recordings out of Git.

Prepare a separate local source checkout and `/workspace/universal-mouse-app` on the existing Box. The currently installed app remains available while the port is developed. Record the baseline source hashes and tool versions. Do not publish a native release or claim that the port is already implemented.

Exit gate: repository contains a reproducible reference and a reviewed feature matrix; Box source checkout matches the pushed commit.

### 1. Protocol core and native diagnostic executable

Port discovery, descriptor parsing, setting decode/merge, report construction, revisions, bounded transfers, stable snapshots, backup and Restore to C++ before wiring hardware writes into the GUI.

Bring across exact byte behavior from the Python/Go tests: preserved unknown fields, lighting/action bit boundaries, stage enable rules, maximum lengths, expected-revision checks, at least one left click, successful write ordering, partial failure, disconnect, interrupted transfer and selective rollback. Keep raw arbitrary reports out of the public command interface.

Build and test a mock-transport native CLI on Box. Compare the native results against the legacy reference and fixture hashes. Do not remove legacy code until its behavior is covered.

Exit gate: native protocol results match reference fixtures; negative/failure paths refuse or recover safely. Tests never require the physical mouse on Box.

### 2. Native GUI and preset persistence

Implement the connection view, Buttons, Sensitivity, Lighting and Macros tabs, device-capability states, native controls, local profiles, Save versus Apply, Undo, pending changes, refresh and error messages. Preserve readable/unsupported current settings without forcing them into an allowed edit range.

Add explicit legacy JSON export/import, versioned validation and atomic saves. Import stages changes only; it does not apply mouse settings or enable macros automatically. Test close/cancel/discard decisions and reconnect while editing.

Exit gate: every supported setting can be represented and edited in mock mode; saved profiles round-trip; Apply and Save remain distinct; the GUI has no embedded browser and no backend subprocess chain.

### 3. Linux integrations and complete macro lifecycle

Replace KWin and remapper status helpers with native D-Bus calls. Preserve per-device KDE settings and revision/readback/rollback behavior. Keep global GNOME/Cinnamon and X11-only imwheel scope accurately labeled, with existing ownership/conflict checks.

Port macro validation/export, exact mouse-group matching, private preset storage, fixed start/stop invocation and actual injector-state confirmation. Test failed startup, CLI timeout after possible startup, stop remaining RUNNING, shutdown during Apply, disconnect and app exit using private/mock services. Keep macro activation explicit and autoload disabled unless the user later chooses it.

Exit gate: integration tests pass without touching the working desktop or physical input devices. Native application helpers contain no Python; externally installed Input Remapper remains a declared dependency.

### 4. Viewer and performance work

Port the generic 2D geometry, selectable controls, 3D mesh/camera/picking, local input tester and lighting/timeline previews. Use software rendering on Box. Ensure reduced motion, hidden tabs and static views stop their timers/rendering. The vestigial Photos control can be removed because the reference bundles no photos.

Measure legacy versus native startup, idle RSS/PSS, idle CPU, window responsiveness, rendering cost and artifact size on the **same Box** with equivalent settings. Record Box's software-rendering limit. Use lightweight final Haswell-rendering checks locally; do not claim a performance result before measuring it.

Initial goals: native idle PSS below 100 MiB, idle CPU below 1% after settling, and clearly lower memory/startup overhead than the legacy build. These are targets, not measured guarantees; revise transparently if the renderer or platform requires more.

Exit gate: feature parity does not regress for optional 3D; measured results and limitations are recorded.

### 5. Hardware validation, packaging and first release

After Box tests pass, transfer the compiled Linux artifact to the local PC. Run lightweight read-only identity/settings checks first, then controlled reversible write/readback/Restore checks against private original backups. Use the existing polling test as the first transaction comparison. Device tests cannot be substituted by mocks or cloud compilation.

Run rendering, focus and input automation only on the separate local Wayland screen. Do not move the working pointer, focus or windows to test the app. A physical macro test needs a design that confines generated input to the isolated receiver; otherwise leave it explicitly unverified until the user performs that check.

Build packages on Box: a Linux binary/archive and Ubuntu/Kubuntu package with a desktop entry, scoped udev rule, Qt/runtime dependencies and license notices. Start with dynamic Qt; no source-built Qt and no unsupported cross-platform release claims. Keep the native install separate until final checks pass, then replace the launcher with an available rollback.

Exit gate: all supported features have parity coverage, installation/uninstallation works, the local mouse is restored after tests, experimental support is labeled, and the package provenance/licenses are complete.

### 6. Additional devices and platforms

Add each mouse as an evidence-backed driver with identity, descriptor, read/write fixtures and capability limits. Shared VID/PID does not establish a shared protocol. Add Windows/macOS adapters only with a corresponding build environment and physical validation; portable C++/Qt source alone is insufficient.

Onboard macros/profiles and missing RGB modes get separate research milestones with captured vendor behavior and narrowly bounded implementation tests.

## Box build workflow and resource policy

The existing `main` Box has **2 vCPUs, 4 GiB RAM and 20 GiB storage** (about 18.7 GiB free during planning), Ubuntu 24.04.5 x86-64 and GCC 13.3. It has no exposed GPU or physical GM320. During planning it had GCC/Git/rsync, but no Qt development packages, CMake/Ninja or Rust toolchain.

1. Clone the public repo into `/workspace/universal-mouse-app`. Source edits and review may happen locally; sync only reviewed tracked source, never the whole home folder, existing desktop-customization workspace, credentials, device settings or dependency/build directories.
2. On Box, install distro build packages: GCC/build-essential, CMake, Ninja, pkg-config, Qt 6 base/SVG development packages, D-Bus tools, ccache and isolated GUI-test dependencies. Add Qt OpenGL and optional desktop adapters only when required. Confirm the supported Qt minimum against distro APIs; start with Qt 6.4-compatible code.
3. Use `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo`, then `cmake --build build --parallel 2`. Start without LTO, set a roughly 1 GiB ccache limit and keep a single active build configuration. Reduce to one job if memory pressure appears.
4. Run unit/integration/GUI tests on Box. Sanitizer configurations run separately/sequentially; remove obsolete build trees to stay within disk limits. Do not enable an automatic public CI build outside Box while the user requires all resource-heavy jobs there.
5. GUI checks use Qt offscreen when sufficient, or a private Wayland/Xvfb screen with isolated session bus/config/runtime. Use software OpenGL and mock transport; no automation on Box's interactive Chrome/session during tests.
6. Run full suites, package assembly, asset conversion and benchmarks on Box. Transfer only release artifacts and concise test reports back. Local identity/device/visual smoke checks remain small and isolated.
7. Save pushed work/results and hibernate Box when the current build session is finished. Do not upgrade the existing Box image or start additional cloud machines unless needed and authorized; neither is required for this plan.

Build-tool packages and generated native targets are installed/implemented in milestone 1. No native compilation is performed as part of the planning-only repository setup.

## Risks and review points

- Qt makes the UI feasible quickly, but custom geometry, small-screen layout and accessibility still need native validation.
- The language port cannot validate undocumented GM320 behavior. Preserve disabled/unverified features and distinguish stored readback from physical effect.
- Box can validate the software and packages, but cannot prove mouse I/O or Intel GPU behavior.
- External desktop/remapper services can change; isolate adapters and test real service-state transitions as well as mocks.
- The name must not overpromise support. The README and capability matrix must identify exact supported hardware and tested platforms.
- Keep the legacy implementation as a rollback/reference until the native release passes its gates. No arbitrary completion date is promised before the first native vertical slice is measured.
