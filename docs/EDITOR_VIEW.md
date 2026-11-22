# Editor view navigation

Signed: President, 2026-11-22.

Middle click samples the active tool: terrain shape/material association, entity template with instance overrides, event helper, or decoration appearance. Select/Inspect can sample the target and arm its corresponding placement tool. A six-pixel movement threshold distinguishes a click from a middle-button drag. Middle dragging pans, including from a focused panel; it never samples. Focus loss cancels the gesture.

Ctrl + mouse wheel over the level zooms around the pointer from 20% to 1000%. View offers fixed zoom values and a 100% reset. Panel scrolling stays available without Ctrl-wheel over the level. WASD/arrows move the view while a panel is merely focused; active controls, text entry and modal dialogs retain their keys. Shift accelerates keyboard panning. The view is independent of the gameplay Camera and resets to 100% on editor entry.

Esc or right click cancels a pending first line point without painting, erasing or resuming play. Once the input is released, normal tools resume. Palette drag release over the canvas places exactly one undoable instance; invalid drops and Esc cancel it. Click-only palette selection still arms placement.

Terrain, entities, grid, previews and drops use the same view mapping. Zoom changes a scoped GPU projection, not the window resolution or the physics. Fine grid display density adapts at wide zoom; the authored brush/grid does not change.

The focused native editor-contract check covers inverse mapping and limits, preserved gameplay camera, line-preview cancellation, render snapshots at 20%/1000%, real palette row drag/release, exact placement count, undo/redo, protected saving and editor re-entry. Physical-device usability remains a manual review.

Gameplay-camera expansion is a separate planned surface. Existing SurgeScript Camera position, lock/unlock and coordinate APIs are the starting bridge. Plan look-ahead from facing/velocity, dead zones, focus targets, authored interpolation, fixed viewpoints/cutscenes and eventual gameplay zoom as one camera authority. Preserve that controller state across editor entry/exit; editor navigation must never write into it. Review earlier camera prototypes when that work begins.
