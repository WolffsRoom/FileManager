# v0.5 Selection, Scroll and Settings Design

## Intent

Match the initial File Manager list motion across every folder, replace unreliable browser checkboxes with native persistent row highlighting, and finish the Settings visual language with explicit toggles and correctly sized icons.

## Behavior

- Before forward or back navigation, an open search ends editing, clears its text/filter, restores the full list, then closes before the folder transition begins.
- Every browser ListView uses the initial `Popup1` row-start animation. Recycled rows are reset on stop so transforms do not accumulate in large directories such as `app`.
- Browser selection never displays checkbox assets. A marked row keeps the same native focused/highlighted state used by the cursor; the actual focused row stays highlighted too.
- Settings choice pages show `toggle-on.png` for the active value and `toggle-off.png` for inactive values.
- Navigation Animation uses `AnimOptions.png` in the Settings main page and submenu.
- All Settings list icons use 90% of the former default size and remain centered. Power action icons retain their existing size.
- Power actions regain a visible native button boundary with a semitransparent background.

## Chosen approach

Use ScePaf's own `SetFocusedState` for persistent browser marks rather than a custom tint, because it most closely matches the cursor. Restore `Popup1` through the existing item-factory start/stop lifecycle rather than adding manual scroll animation. Keep settings toggles in the existing right-side row plane so no new list template is needed.

## Constraints

- Preserve main-thread PAF safety and recycled-row guards.
- Preserve existing VPKs and create a uniquely named v0.5 test package.
- Update branch `test` only; no GitHub Release.
