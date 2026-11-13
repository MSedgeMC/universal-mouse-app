# GM320 Mouse Settings for Linux

> Historical source baseline for the native port. This README records the original local app's state; paths and PC-specific verification describe that setup. See the [repository README](../../README.md) and [migration plan](../../docs/MIGRATION_PLAN.md) for current project status. Compiling and full testing run on Box.

Local MIT-licensed adaptation of [Resa](https://github.com/RajanDhamala/fantech-thor-ii-x16v2-linux), pinned in `UPSTREAM.json`, for the connected Ant Esports GM320 (`30fa:1440`, INSTANT USB GAMING MOUSE).

Open **GM320 Mouse Settings** from the application menu after running `python3 scripts/install-user.py`. The installed build is under `~/.local/lib/resa-gm320`. It runs as your normal desktop user.

## Controls

- Hardware settings: seven button slots including Disabled, six DPI stages, supported polling choices, captured lighting effects/colors/brightness/speed, and device timing. Edits require **Apply changes**. The app first reads current settings, validates the revision and saves a private backup of changes; failed writes attempt restoration.
- Linux macros: letters, numbers, F keys, modifiers and common navigation keys, key down/up events and explicit delays. Choose Back, Forward or Middle click, then **Apply Linux macro**. Sequences must balance every press with a release and stay within 30 seconds. This starts an Input Remapper preset; **Stop Linux macro** restores ordinary events. Macros stop on normal app shutdown and are not stored onboard.
- **Save preset** saves edits locally without applying hardware changes. Four local presets include hardware settings and one Linux macro each. Selecting a preset stages it; Apply changes sends its hardware edits, and Apply Linux macro enables its macro.
- KDE sensitivity and native scroll factor apply only to the matching GM320 pointer and are read back through KWin. They persist after the app closes. Other desktops retain upstream behavior where supported.

Input Remapper is a separate dependency installed with `sudo apt-get install input-remapper`. Its service must be running. Version 2.2.0 is installed and its service is enabled on this PC. The application runs as your normal desktop user. Buttons consumed internally by firmware cannot trigger Linux macros. A crash or forced process kill may leave the Input Remapper preset active; stop it in Input Remapper or reconnect the mouse.

The installed `/etc/udev/rules.d/70-resa-gm320.rules` grants the active local session access to this mouse's configuration interface and input-event collections. This lets the normal-user Input Remapper CLI discover the mouse without adding your account to the broad `input` group. To install on another PC, copy `udev/70-resa.rules` to that path, reload udev rules and reconnect the mouse.

## Verified on this PC

The GM320 configuration descriptor matches the pinned 140-byte descriptor exactly. Stable settings reads and the unchanged-apply path are checked; a reversible polling test also confirmed write/readback and full restoration. The initial read reports enabled stage 2 at 1,600 DPI and a stored 1,000 Hz polling choice; these are decoded configuration values, not physical measurement. Existing button slot 7 is disabled. Hardware profile activation and the meaning of every GM320 physical button are not established by these seven stored slots.

The Electron app was visually tested with an isolated screen; this repository provides `../../tools/virtual-screen.py`, with its own Wayland socket, configuration and D-Bus. GUI tests edit/save local macros only, capture screenshots to `evidence/gui/`, and do not send input or change hardware settings. Go, Python and JavaScript tests validate protocol allowlists, preservation, backups/rollback, macro grammar, bridge requests, shutdown handling and KDE level mappings.

The installed Input Remapper package parses all 67 offered keys and validates the generated preset. Running a Ctrl+C sequence with an in-memory event recorder confirms its balanced events and 100 ms delay; no keystrokes are injected. Its system D-Bus status endpoint works and its CLI detects the exact mouse group. Actual button-triggered remapping on the working desktop remains untested and is enabled only when you click **Apply Linux macro**.

## Remaining limits

This is experimental GM320 support. Configuration reading works on this mouse, and a polling change from 1,000 to 500 Hz was written and read back successfully, then restored with an identical baseline revision. Actual report frequency and reconnect persistence have not been measured. Other hardware edits remain unverified. Some lighting effects stay unavailable until there is supporting evidence. Onboard macro upload, onboard keyboard programming and hardware profile switching are not implemented. Linux software macros provide shortcuts while their service is active. Device timing differs from Linux's double-click threshold and is not validated for the GM320.

The seven-button illustration is a generic adaptation of Resa's X16v2 geometry, not a photograph of this GM320. Upstream personal reference photos are excluded.

## Build and verify

```sh
pnpm install --frozen-lockfile
node node_modules/electron/install.js
make build
make test
pnpm check
node scripts/build-preload.js
python3 scripts/install-user.py
```

Go 1.22+, Node 22.12+ and Python 3 are required for building. The installed application includes its compiled Go helper and Electron runtime and needs Python 3. Electron's sandbox remains enabled.

The original MIT license is preserved in `LICENSE`. `UPSTREAM-README.md` and `PROTOCOL.md` retain upstream documentation and evidence limits; current local behavior is described here.
