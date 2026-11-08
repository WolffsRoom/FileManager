# File Manager v0.5 - Native Navigation, Selection and Search Design

## Intent

Start v0.5 from commit `aa58cbb`, using `build/FileManager (alpha).vpk` as the authoritative reference for folder advance, return and list scrolling. Fix the Settings crash and selection/search regressions without changing the expected Vita-native interaction.

## Verified reference

`build/FileManager (alpha).vpk` and `Release/FileManager-alpha.vpk` are identical (SHA-256 `46272F0938D7AE77C2B00B514F22C09120FA6BA075690D24D34D5715AC419511`). Its matching early implementation sends the parent list to `Type_3D_SlideToBack1`, brings it back with the native reverse transition, and lets the ScePaf ListView own scroll motion.

## Chosen approach

Restore the alpha/ScePaf navigation sequence as the `Padrão` mode while retaining only the safety needed by the expanded application: filesystem loading stays off the UI thread, PAF mutations stay deferred to the UI thread, and pages are freed after their transition callback rather than by frame-count geometry correction. Keep the alternate fade mode under the name `Simplificado`.

The rejected approach is to keep tuning the current custom 20/24-frame transition controller. It has already produced half-screen geometry, hidden parent lists and different forward/back behavior, and it overrides state that ScePaf normally owns.

## Required behavior

- Opening a folder moves the previous page to the native left/back position seen in the alpha VPK.
- Returning reverses that same transition and restores the previous page at its prior focus/scroll position, without manual half-screen transforms.
- Normal scrolling uses the alpha ListView behavior and does not accumulate recycled-row animations.
- Settings no longer crashes. The checkbox lookup must follow the new template hierarchy where the checkbox is a sibling of the image button.
- Navigation labels are `Padrão` and `Simplificado` in Portuguese, with equivalent concise labels in every supported language.
- Selection uses four slightly narrower bottom buttons and leaves the lower-right `...` control visible and above the selection surface.
- Checked state has one shared lookup/update path for newly created, visible and recycled rows.
- Opening Search first focuses the list head, then opens the keyboard. `CB_TEXT_BOX_STRING_CHANGED` filters incrementally without ending keyboard editing; Enter or Circle closes the keyboard as already requested.
- Existing v0.4 VPKs remain untouched. New packages go to a v0.5 test directory; no GitHub Release is created.

## Validation

- Add a regression for the Settings template hierarchy and the supplied crash mapping (`SettingsMenu::CreateItem` calling ScePaf with a null checkbox widget).
- Add navigation checks against the alpha transition sequence and absence of frame-count geometry correction.
- Add selection tests for button layout, options visibility and row checkbox consistency.
- Add search tests for head focus and incremental filtering while editing.
- Run all existing regression scripts, compile a Release VPK, verify ZIP integrity and preserve prior artifacts.
- Hardware validation remains required for exact ScePaf animation timing and touch appearance.
