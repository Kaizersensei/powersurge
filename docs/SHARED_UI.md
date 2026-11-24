# Shared UI pilot

PowerSurge owns the widget interfaces and state identities; Dear ImGui supplies layout, interaction and rendering. The editor keeps its neutral font and colors. The game pilot uses the original C2 parchment and Caveat Brush family. This is a focused pilot, not a migration of every menu.

## Try it

Open a level, press F12, and choose **Test > Laboratories > Shared UI pilot**. Confirm that it will replace the level. This runs in the same engine instance, with its own disposable content, progress and preferences. Alternatively run `powersurge.exe --lab ui-pilot` from the runtime directory.

- T opens the existing conversation; Shift+T opens its 64-choice test.
- Up/Down or W/S navigate, Enter/H confirms, Esc/J cancels. Mapped gamepad directions and A/B use the same actions. Click an option to select; click it again or double-click to confirm, or use the visible Confirm/Cancel buttons. The pointer is visible in the in-game pilot; the wheel scrolls choices without forcing the previous selection back into view.
- Message, choices and selected-choice information have separate parchment windows. The top strip stays fixed while the bottom opens/closes; the middle is a nine-patch region.
- The upper-right pilot panel switches to the previous renderer for comparison when a conversation is closed. Its MP slider is a labelled fixture; HP reads the controlled actor's actual resource.
- F12 opens the normal level editor. Select a BOR Pole and use Properties: numeric and boolean controls retain instance overrides, effective runtime values and Reset to template. Existing undo, validation and save ownership remain with the placement provider.
- F1 in the editor shows searchable command bindings and quick tips. The same command definitions supply shortcut handling and menu captions.

## Ownership and data

`src/ui/widgets.h` is the presentation boundary. A conversation carries an actor lifetime ID and a conversation epoch. The widgets emit selection, confirm or cancel intentions; the foundation host validates current ownership/epoch and remains the only outcome, event and policy owner. Old, cross-owner, closed and duplicate intentions are refused. Navigation plus confirmation uses the new selection in the same batch; cancel takes precedence. Input release is required after opening, and the existing host barrier protects gameplay on close.

The game skin and reference-space layout are authored in `labs/content/ui/widgets.ini`. Its dimensions scale from 1920x1080 to the active display; editor interface scale is independent. Missing optional editor appearance configuration uses defaults; invalid numbers use defaults. Missing game skin assets retain the legacy renderer with a diagnostic. Gamepad controls use `inputs/shared-ui.in` through the engine's device mapping, rather than raw hardware indices. No platform-specific XInput GUI dependency is introduced.

The meter uses all original 26 art frames to derive a continuous per-pixel reveal order. Runtime values interpolate that mask; rendering does not choose a discrete animation frame. Full and empty endpoints preserve the original rasters. The two 48x64 pieces use native pixel dimensions, nearest sampling and a whole-pixel offset to form the yin-yang outline. `meter.scale` defaults to 1; optional integer magnification avoids fractional stretching. The generic HP range is validated independently of the fill. MP has no authoritative service binding in this slice and must not be presented as gameplay MP. One active player viewport is the supported pilot scope.

Widget submission requires an active shared frame. Scene teardown clears pending intentions and identities before resources are released. UI animation follows the existing UI clock and reduced-motion setting; it does not change simulation, calendar or audio pause ownership. Pilot activation is cleared when switching to another laboratory.

## Native verification

`powersurge.exe --lab ui-pilot-check` runs the native UI/compatibility scenario without Python or an external test framework. It checks absent/malformed optional configuration, authored meter endpoints and intermediate fills, command identities, simultaneous navigation/confirm, cancellation, closed/stale/cross-owner intentions, no-frame submissions, the real conversation controller and 64-choice scrolling through owned keyboard/gamepad adapter events, plus pointer visibility, mouse-wheel browsing and click/select/confirm. Its service checks exercise version 1/2/3 progress paths, owner-specific ability events, checkpoint rebinding and failed/corrupt/newer save preservation. Generated legacy-format fixtures are not certification of every historical user save or execution of old binaries.

The broader `--lab frontend --lab-switch-check` journey includes the pilot, ordinary terrain, physics, editor interactions and repeated connected-level travel in one process. It checks that pilot activation and state do not escape into ordinary play. Runs use temporary slots, never normal player saves; normal cleanup and bounded diagnostic retention apply. Rendered evidence stays private.

Physical gamepad/hotplug feel, accessibility and additional viewports require hands-on review. Inventory retains its C2 renderer and authored layout, with clickable slots, category arrows, Use/Equip, Assign, Back and Resume. Its pointer targets come from the rendered/clipped catalog geometry, transformed through the actual engine display mapping. Commands carry menu owner/revision and stable item IDs; stale requests and held clicks across a replaced surface are rejected. The native case exercises real healing/consumption, simultaneous mouse and keyboard confirmation, and close cleanup. Other game menus retain their existing renderer. Soundboard authoring and broader UI migration remain separate tasks.

Signed: President.

Choice lists no longer add a blank trailing row. The pilot controls hide while a gameplay menu is open so they cannot obstruct category buttons or slots. Signed: President.
