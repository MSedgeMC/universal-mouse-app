# Universal Mouse App instructions

- Port the complete supported application to C++20 with Qt 6 Widgets. Preserve behavior and protocol safeguards before expanding device support.
- Compilation, dependency installation, code generation, full tests, sanitizers, benchmarks, asset processing and packaging must run on the user's Simular Box. Do not run them on the user's PC.
- Use `/workspace/universal-mouse-app` on Box. Start with two parallel build jobs, prebuilt distro Qt, no LTO and a bounded build cache. Do not compile Qt from source.
- Keep tokens, private settings, device backups, live dumps, raw USB captures, vendor installers and generated artifacts out of Git and out of source transfers. Send only reviewed project files to Box.
- The native program must not depend on Electron, Node, Go or application Python helpers. Input Remapper remains a separately installed external service with its own Python runtime.
- Keep hardware I/O off the GUI thread. Unknown hardware receives discovery information only. Require exact supported identity/interface/descriptor checks before writes; preserve unknown bits, revisions, stable reads, last-left-click protection, write ordering, private backups and rollback.
- Box has no physical GM320 and no exposed GPU. Use mock HID and software rendering there. Actual device and Haswell-rendering checks run locally only after Box checks pass.
- Keep all test windows, screenshots and automated input off the user's working desktop. Native Wayland tests use `python3 tools/virtual-screen.py -- <command>`; Qt offscreen is acceptable when a compositor is unnecessary. A test screen must have its own display socket, session D-Bus, runtime, config/data directories. Never restart or replace the working compositor or connect test input to its display/session bus.
- Inspect screenshot artifacts directly, clean up the test screen/processes, and never launch image viewers on the working desktop.
- Use the user's Ubuntu Medium interface font and readable native controls. Do not claim an exact macOS material or renderer.
- Keep MIT provenance and applicable Qt/external dependency notices. Describe experimental and unimplemented capabilities accurately; the repository name is not a support guarantee.
