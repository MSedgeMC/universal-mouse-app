# Original native design — 0.2.0

The native dashboard uses original MSedgeMC visual code and source assets: a charcoal/mint palette, semantic text roles, device sidebar, compact assignment rows, colored DPI stage cards, a fixed Save/Apply footer, hand-drawn line icons, and eight-control mouse schematics. The normal interface has no attribution banners. The native package uses no legacy artwork, device photographs, vendor logos, or copied stylesheets.

Public references reviewed on 2026-11-13:

- [Logitech G HUB](https://www.logitechg.com/en-us/innovation/g-hub.html): connected-device cards, strong device identity, concise navigation, profile context.
- [Razer Synapse 4](https://www.razer.com/synapse-4): dark neutral surfaces, device configuration sections, selective accent color.
- [SteelSeries Engine](https://steelseries.com/gg/engine): DPI stages, distinct response/lighting sections, visible effect and palette controls.
- [Ant Esports GM320 product](https://antesports.com/products/gm320-8-button-mouse-braided-cable-black): physical layout reference. The user confirmed the small fire button beside left click as the eighth button. Upstream marketing is internally inconsistent about programmable button counts; it does not verify the hardware register mapping.

These sources inform interaction and hierarchy; their screenshots are research references and are not included in the app. Mouse diagrams are schematic and do not claim exact model geometry.

Eight physical buttons and seven verified assignment registers are represented separately. No eighth write address is guessed. The independent fire button remains visible and marked Unverified for remapping. The captured action, DPI, lighting and backup protocol is preserved.

A new interface does not turn derived code into a clean-room implementation. The hardware port and retained legacy source keep the required MIT notice. Original MSedgeMC additions are identified separately in LICENSE. Qt and Input Remapper retain their own terms. A completely independent backend would require a separately reviewed protocol specification and fresh implementation/validation; removing artwork alone does not remove code attribution.
