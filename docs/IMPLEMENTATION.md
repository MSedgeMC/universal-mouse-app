# Native architecture

`src/core` contains the captured protocol model, strict JSON/profile validation, versioned private storage, and transaction/readback/rollback logic. `src/drivers` discovers Linux hidraw endpoints and rechecks identity and descriptor on the opened device. `src/platform` integrates desktop controls and the external Input Remapper service. `src/ui` uses Qt Widgets, a serialized QtConcurrent work queue, and a software-projected QPainter mouse preview. `src/main.cpp` provides GUI and bounded headless diagnostics.

No browser engine or former helper runtime is used by the native executable. Development oracle scripts run separately. Input Remapper remains external. The initial driver intentionally handles only the captured Instant descriptor; adding a device requires its own reviewed protocol and matching rules.

The original implementation stays in `legacy/resa` to preserve provenance, protocol fixtures, and a migration path. Its only native-port bridge change is a saved-preset JSON export menu. Native hardware backups accept legacy versions 1–4 when their exact allowed fields and identity match.

The native GUI uses C++ geometry and software shading instead of porting the browser WebGL renderer. It provides 2D/3D selection, orbit/zoom, keyboard navigation, auto rotation, event counts, and illustrative lighting. It does not require a GPU context. Optional animation runs only while the relevant preview is visible.

Release 0.2 uses an original theme/resource bundle, QPainter icon set, and newly drawn mouse schematics. No legacy asset is referenced by the native build or installer. The eighth physical fire button has a display/selection region but no writable protocol field. Its addition does not alter profile version 2, backup addresses, descriptor matching, or revision hashing.
