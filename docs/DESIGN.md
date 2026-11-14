# Native mouse interface — Orbit 0.4.0

The user selected **Razer Synapse's mouse layout with Logitech G HUB's black/gray/blue coloring and polish** after reviewing actual mouse-settings screenshots, then requested the **original GM320 product image**. The native Qt implementation follows that hierarchy with original controls, theme code and icons. Its font remains Ubuntu Medium. The official Ant Esports device image is bundled with its own provenance notice; Razer and Logitech assets and stylesheets are not included. This is independent software with the hardware scope documented in README.

## Interface

- A compact black brand bar, device/profile context, and horizontal Customize / Performance / Lighting / Macros / Desktop navigation.
- Customize centers the official GM320 product image on a subtle dotted canvas. Eight mouse and callout targets open a side assignment editor only when selected. Top and detailed GM320 3D views share selection and local input testing.
- Performance shows six DPI tiles with enable/active controls, a selected-stage slider/editor, report rate and click timing. Editing a stage is separate from choosing the active stage.
- Lighting keeps the preview and effect settings together, followed by a separate live-color panel, seven-slot palette and advanced controls.
- Macros uses numbered key-down, key-up and delay rows, a native event editor and explicit trigger/enable/stop controls. Desktop pointer and scroll settings have their own page.
- A fixed footer exposes Restore, status, Discard, Save preset and Apply to mouse. Local saves and hardware writes remain separate.

The shell uses near-black `#0b0b0d`, charcoal `#202124`, panel `#141517`, restrained gray borders and blue `#159bff`. Rounded navigation and segmented controls, consistent spacing, original line icons and clear typography carry the premium direction. Essential assignment, DPI and macro controls fit 1040×720; the longer Lighting page scrolls to its palette. The preferred size is 1280×820, capped to the desktop's available area at startup. A 1040×680 minimum and page scrolling accommodate shorter displays while the action footer stays visible.

## Research references

Mouse-settings screenshots reviewed on 2026-11-14, retained only as local ignored research artifacts:

- [Razer Synapse 4, DeathAdder V4 Pro](https://tpucdn.com/review/razer-deathadder-v4-pro/images/software1.jpg): profile/device header, horizontal device sections, centered mouse and button callouts with a contextual assignment editor.
- [Logitech G HUB, PRO X SUPERLIGHT 2](https://tpucdn.com/review/logitech-g-pro-x-superlight-2/images/software2.jpg): neutral dark surfaces, blue accent and focused DPI presentation.
- [SteelSeries Engine, Rival 5](https://tpucdn.com/review/steelseries-rival-5/images/software1.jpg): comparison shown before the user's choice.
- [Ant Esports GM320 product](https://antesports.com/products/gm320-8-button-mouse-braided-cable-black): physical layout reference. The user confirmed the small fire button beside left click as the eighth control. Marketing button counts do not establish register mappings.

The top view is a transparent crop of the official GM320 product image, retaining the mouse shape and printed branding. Lighting uses a neutral derivative and alpha mask from that same image to illustrate color and motion without claiming firmware parity. The detailed GM320 mesh is reconstructed from the pinned top image and side/oblique product views, including the asymmetric thumb rest and side fire switch. Camera depth and unseen underside details are inferred; the model is not measured CAD. See [model provenance](../assets/GM320-MODEL.md). Static and hidden previews stop their animation timers; scaled image captures are cached by size and device scale, and the color layer is cached until its preview color changes. Source, revision and reproducible preparation are recorded in [GM320 image provenance](../assets/GM320-IMAGE.md).

## Hardware and provenance

Eight physical buttons and seven verified assignment registers are represented separately. The fire button remains selectable with remapping unavailable. No eighth address is guessed, and the redesign leaves the existing action, DPI, lighting, revision and backup protocol intact.

Normal settings pages have no attribution banners. The hardware port and retained legacy source preserve the required MIT notice; changing the UI does not remove source provenance. Original MSedgeMC additions are identified in LICENSE. The official GM320 image is excluded from the MIT grant and retains Ant Esports ownership. Qt and the external Input Remapper service retain their own terms.

Orbit uses a compact orbital mark in the original blue palette. The public repository URL and package IDs remain stable for existing users. The 3D camera presets include top, left, right and front views; software depth/ID rendering prevents selecting a switch through the shell.
