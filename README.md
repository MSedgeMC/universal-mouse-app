# Universal Mouse App

A native **C++20 / Qt 6 Widgets** mouse configurator for Linux, starting with the Ant Esports GM320 / Instant `30fa:1440` controller. No Electron, Node, Go, or application Python runtime is required.

**Status: experimental native release 0.1.0.** The native app runs on KDE Wayland and has been checked with a physical GM320. The repository name describes an extensible driver architecture; it does not promise support for arbitrary mice. Configuration writes require the supported USB identity, interface, and exact HID descriptor.

## Features

- Seven button assignments, including Disabled, with last-left-click protection.
- Six DPI stages, enable/active-stage controls, stored polling selection, and device click timing.
- Captured lighting settings: palette, slot enables, brightness, speed, supported effects, direction, symmetry, and button responses. Unsupported readback values are preserved.
- Experimental live Static/Breathing/Off overrides, with illustrative animated previews.
- Four local presets with separate Save and Apply actions; JSON import/export, undo, stable readback, private backups, rollback, and selective restore.
- Linux keyboard macros: 67 keys, down/up/delay events, sequence preview, and three mouse triggers through the external Input Remapper service.
- GM320-specific KDE pointer acceleration and scroll factor; GNOME/Cinnamon pointer speed and optional X11 imwheel integration have narrower scope.
- Native 2D and software-rendered 3D previews, orbit/zoom, keyboard navigation, auto rotation, and app-local input testing.

Onboard macros, onboard keyboard programming, hardware profile switching, additional mouse drivers, and complete vendor RGB parity remain unimplemented. Polling selection is a stored choice; this app does not measure the physical report rate. Most button/lighting writes still need physical behavior measurements.

## Install

Download an archive or Debian package from [Releases](https://github.com/MSedgeMC/universal-mouse-app/releases). The first binary targets **x86_64 Ubuntu 24.04 or newer**, dynamically linked against system Qt 6.

For a system install:

```sh
sudo apt install ./universal-mouse-app-0.1.0-Linux.deb
```

For a user install, install the Qt runtime packages (`libqt6widgets6`, `libqt6dbus6`, and `qt6-wayland`, or their distribution equivalents), extract the archive, and run:

```sh
./universal-mouse-app-0.1.0-Linux/usr/share/doc/universal-mouse-app/install-user.sh
```

Start **Universal Mouse App** from the application menu. Run `universal-mouse-app --mock` for a demo with no hardware or desktop writes. The app must run as a normal desktop user.

The Debian package installs a narrowly scoped udev rule. For an archive installation without existing mouse access, install `usr/lib/udev/rules.d/70-universal-mouse-app.rules` into `/etc/udev/rules.d/`, reload the rules with `sudo udevadm control --reload-rules`, and reconnect the mouse. Do not grant access to every input device or run the GUI as root.

Linux macros require a separately installed, running **Input Remapper 2.2.0** service. Only one app should manage its mapping for the mouse. Macros stop on normal app closure; a forced exit may leave the service mapping active. See [usage and migration](docs/USAGE.md).

## Build and verify

Dependencies: C++20 compiler, CMake 3.22+, Ninja, Qt 6.4+ Core/Gui/Widgets/DBus/Concurrent, and Qt Test for tests. Python is used only by development verification scripts. Packaging also needs `file` and Debian packaging tools.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel 2
# Run GUI tests inside an isolated session with private XDG directories.
python3 tools/box-verify.py
python3 tools/box-tests.py
python3 tests/parity.py
cmake --build build --target package
```

This project's implementation builds, dependency installation, full tests, sanitizers, measurements, and packaging were performed on Simular Box. Local checks were limited to the real device and isolated native Wayland rendering. See the [validation report](docs/VALIDATION.md), [migration plan](docs/MIGRATION_PLAN.md), and [source baseline](docs/SOURCE_BASELINE.md).

## License

[MIT](LICENSE), preserving Resa author Rajan Dhamala's attribution. The previous Electron/Go/Python app remains under [legacy/resa](legacy/resa/README.md) for protocol comparison and migration. Upstream revision: `fc12318e20c694cb1bf9965e2db0bce9514be94d` from [Rajan Dhamala / Resa](https://github.com/RajanDhamala/fantech-thor-ii-x16v2-linux).

Qt is dynamically linked and retains its own licensing. Input Remapper is an optional, separately distributed GPL service. See [third-party notices](packaging/THIRD_PARTY.md).
