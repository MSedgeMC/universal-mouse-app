# Usage and migration

Read mouse settings first with **Sync device**. Edit the selected local preset, then choose **Save preset** to save that preset on disk or **Apply to mouse** to write changed supported fields and verify readback. Saving and switching presets never write the mouse. The first run initializes all four presets from the connected mouse; later runs preserve saved presets. Reload adopts untouched fields while retaining staged edits.

Settings are stored in `~/.config/universal-mouse-app/` (or `$XDG_CONFIG_HOME/universal-mouse-app/`) with private files. **Discard** reverts the selected preset to its saved local copy. The last hardware transaction has a versioned `device-backup.json`. **Restore** restores only fields changed by that transaction. A new hardware transaction replaces the last-change backup. After a failure, sync before retrying; an error distinguishes successful rollback from an unconfirmed restoration.

In **Customize**, select a mouse button or its callout to open the assignment editor. Close it with its × control. With the mouse preview focused, keys 1–8 select controls and Enter/Space opens the editor. The GM320 has eight physical buttons, including a small fire button beside left click. Only seven configuration addresses are verified in the captured protocol. The fire button is selectable with its assignment disabled and marked **Remapping unavailable**; no guessed eighth address is read or written. **Top view** uses the official product image; **3D schematic**, orbit/zoom and the input tester are local previews.

In **Performance**, each of the six tiles contains its stored DPI, enable checkbox, and active-stage radio control. **Adjust** selects the stage controlled by the slider and numeric editor. The editor snaps to the nearest supported DPI. Selecting a stage for adjustment does not change the active stage.

Keep at least one Left click assignment and at least one enabled DPI stage, including the active stage. Device click-timing edits are limited to 8–300 ms. Existing uneditable values remain visible and are preserved unless changed. Timing is a firmware field rather than the desktop's double-click threshold.

Only captured supported lighting effects can be selected. Unknown or unvalidated effects read from the mouse remain displayed without becoming write choices. Speed codes 6/7 and brightness code 7 are readable but uneditable. Live color overrides have no trustworthy effect readback; the message confirms report delivery only. Off temporarily suppresses response flags while preserving desired response choices for turning lighting back on. Clearing a live override reactivates a supported stored effect. Preview animation illustrates colors; it is not a simulation of the device firmware. Static, hidden, and inactive previews do not run animation timers.

In **Lighting**, effect, brightness, speed and direction are shown beside the mouse preview. Live color has a separate panel; scroll to the seven-slot palette and advanced controls. Editing these controls stages the preset until **Apply to mouse**.

In **Macros**, add key-down, key-up or delay events to the numbered sequence. Macros contain at most 64 events and 30 seconds of delays. Each key down requires a matching key up. **Preview sequence** highlights events without sending keyboard input. Choose Back, Forward, or Middle as the Linux trigger and click **Enable macro**. The corresponding mouse button must emit that Linux event; firmware-only light/DPI buttons cannot trigger software macros. The app checks actual Input Remapper state before reporting success. **Stop macro** and normal close stop this app's mapping. Presets and macro data are stored locally rather than onboard.

KDE desktop controls address the matching GM320 pointer through KWin. GNOME/Cinnamon pointer speed affects all mice in that desktop session. Outside KDE, wheel multiplication requires X11 and optional `imwheel`, affects the session while the app is open, and retains 1x modifier behavior. The app declines unrelated `~/.imwheelrc` files or existing external imwheel processes. These optional desktop adapters are not a guarantee of support for every Linux desktop.

## Bring presets from the old app

The retained Electron app has a **Presets → Export saved presets for Universal Mouse App…** menu entry. Press Alt to reveal its menu bar if hidden. Save each desired preset in the old app, then export to a new JSON file. Open the **…** menu beside the native preset selector and choose **Import presets…**. Version 1 exports are converted to native version 2 data; save the desired imported presets individually. **Export presets…** includes all four currently staged presets.

The migration does not read Chromium LevelDB files or upload personal settings. Keep the old app installed until you have checked your presets. On this workstation it remains independently installed as **GM320 Mouse Settings**.

## Remove

User installation: run the packaged `uninstall-user.sh`, or remove the native launcher, binary, desktop entry, and icon. Saved presets/backups remain. System installation: `sudo apt remove universal-mouse-app`. These operations do not uninstall Input Remapper or the retained old app.
