# Universal Mouse App

A planned native desktop mouse configurator, starting with the Ant Esports GM320 / supported Instant controller family. The native implementation will use **C++20 and Qt 6 Widgets**.

**Status: migration planning and source baseline.** This repository does not yet contain a working native executable. The existing Electron/Go/Python application is preserved under [`legacy/resa`](legacy/resa/README.md) as the reference for the port. The installed GM320 application is independent of this repository.

The name describes the intended driver architecture. It does not mean arbitrary USB mice are supported. Hardware writes require a matching device identity, interface and protocol descriptor. Unrecognized mice will receive discovery information without configuration writes.

## First native release

- Button assignments, six DPI stages and captured polling/lighting settings.
- Four local presets, explicit Save versus Apply, backups, readback and rollback.
- Linux keyboard macros through the separately installed Input Remapper service.
- KDE pointer/scroll controls, with desktop-specific support exposed accurately.
- A native 2D mouse view and optional 3D preview with a readable fallback.

The first target is Linux. Onboard macro upload, onboard keyboard programming, hardware profile switching and complete vendor RGB parity remain separate protocol work. A language port does not establish those features or validate additional mice.

## Plan and build policy

Read the [migration plan](docs/MIGRATION_PLAN.md) for milestones, architecture, feature coverage and release gates. The [baseline notes](docs/SOURCE_BASELINE.md) explain provenance and the retained source.

Compilation, dependency installation, full test suites, sanitizers, performance measurement and packaging run on the user's Simular Box. Local work is limited to source editing, review and lightweight final checks with the physical GM320. GUI tests use an isolated screen; they never drive the user's working desktop.

## License and attribution

The application source retains the [MIT license](LICENSE) and Resa author's copyright. Upstream: [Rajan Dhamala / Resa](https://github.com/RajanDhamala/fantech-thor-ii-x16v2-linux), revision `fc12318e20c694cb1bf9965e2db0bce9514be94d`. Qt and external dependencies retain their own licenses; the distribution plan uses dynamic Qt linking and includes applicable notices.
