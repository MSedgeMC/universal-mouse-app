# Source baseline

The `legacy/resa` directory preserves the reviewed GM320 adaptation of Resa before the native port. It provides the application UI, Go discovery/lighting helpers, Python hardware worker, tests, bounded macro integration, KDE helpers and protocol documentation.

Upstream Resa is pinned to `fc12318e20c694cb1bf9965e2db0bce9514be94d`; its MIT license and Rajan Dhamala attribution are retained. [`legacy-source-manifest.json`](legacy-source-manifest.json) records the SHA-256 hashes of copied source files before repository-specific baseline documentation/build adjustments.

Dependencies, compiled binaries, generated preloads, screenshots, compositor logs, current device dumps, private presets/backups, API credentials, raw USB captures, vendor executables, decompilation outputs and unrelated desktop projects are excluded. The generic mouse geometry is retained; upstream personal reference photographs were already excluded.

The support JSON and `linux-discovery.json` are historical public protocol fixtures. Their paths refer to recorded research inputs and simulated/cached device metadata; they are not live discovery results. Some paths in historical `PROTOCOL.md` and upstream documentation name a sibling `api/analysis` research tree or raw inputs which are not included here. They are provenance references, not downloadable artifacts or native build dependencies.

The retained legacy tests are the parity reference. Future native tests must compare protocol bytes and outcomes against these fixtures, rather than only check that a rewritten function behaves like itself. Native migration requires no rewriting of historical evidence as if it were new device verification.

The installed local GM320 application was previously verified for stable reading and a reversible polling write/readback/restoration. Most other hardware effects and actual button-triggered Linux macros remain physically unverified. These limits remain in the native support matrix until additional measurements establish them.
