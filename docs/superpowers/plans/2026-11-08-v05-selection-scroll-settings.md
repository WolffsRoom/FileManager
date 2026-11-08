# v0.5 Selection, Scroll and Settings Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restore consistent native list motion, replace browser checkboxes with persistent cursor-style marks, and refine Settings visuals.

**Architecture:** Centralize navigation search closure and browser mark presentation in `BrowserMenu`. Use the existing ListView factory for Popup1 animation on all pages, and the existing Settings check plane for on/off toggle textures.

**Tech Stack:** C++11, ScePaf, RCO XML, PowerShell regression checks, VitaSDK.

**Spec:** `docs/superpowers/specs/2026-11-08-v05-selection-scroll-settings-design.md`

## Global Constraints

- Keep PAF widget mutations on the UI thread.
- Never overwrite or delete an existing VPK.
- Push only branch `test`; do not create a Release.

## Review Focus

- Closing search during navigation must rebuild the unfiltered list before the transition.
- A recycled marked row must retain highlight without leaving a stale highlight on an unmarked row.
- Popup1 must reset on row stop in large folders.
- Settings main/commands pages must not incorrectly show choice toggles.
- Translated labels must not move toggle or icon alignment.

---

### Task 1: Search closure and universal row animation

**Files:**
- Modify: `src/browser_menu.cpp`
- Modify: `src/browser_menu.h`
- Create: `tests/v05_navigation_search_scroll.ps1`

**Interfaces:**
- Produces: `CloseSearchForNavigation()` and universal item-factory Popup1 lifecycle.

- [ ] Write and run a failing regression for clear/close-before-push/pop and Popup1 on every browser list.
- [ ] Implement `void CloseSearchForNavigation()` and call it before forward/back transitions.
- [ ] Restore Popup1 for every browser ListView while retaining reset in `Stop()`.
- [ ] Run navigation regressions and commit.

### Task 2: Native persistent browser marks

**Files:**
- Modify: `src/browser_menu.cpp`
- Create: `tests/v05_native_selection_mark.ps1`

**Interfaces:**
- Produces: browser selection presentation based on native focused state, with browser check planes always hidden.

- [ ] Write and run a failing regression that forbids browser checkbox presentation and requires marked/focused visual state.
- [ ] Update row fill, toggle, select-all and clear-selection paths to use `SetFocusedState(marked || focused)`.
- [ ] Keep Settings toggle use of the shared row plane unchanged.
- [ ] Run selection regressions and commit.

### Task 3: Settings toggles and icons

**Files:**
- Modify: `resource/file_manager_plugin.xml`
- Modify: `src/settings_menu.cpp`
- Create: `tests/v05_settings_visuals.ps1`

**Interfaces:**
- Produces: `tex_ui_toggle_on`, `tex_ui_toggle_off`, `tex_ui_anim_options`, 90% list icons, and semitransparent bordered power actions.

- [ ] Write and run a failing RCO/settings regression.
- [ ] Register three textures and use AnimOptions for navigation rows.
- [ ] Show on/off toggles on choice/shortcut pages and hide them on non-choice pages.
- [ ] Apply centered `91.8×63` texture size to Settings list icons; leave power icons unchanged.
- [ ] Restore native power button texture at reduced alpha.
- [ ] Compile, run Settings regressions, and commit.

### Task 4: Final v0.5 test package

**Files:**
- Create: `docs/v0.5-test-selection-scroll-settings.md`
- Create: unique VPK in `scripts/build/vpk/v0.5-test/`.

**Interfaces:**
- Consumes: Tasks 1-3.
- Produces: tested v0.5 package on branch `test`.

- [ ] Run the complete PowerShell suite.
- [ ] Build Release v0.5-test and validate ZIP entries/SHA-256.
- [ ] Commit documentation and package without replacing history.
- [ ] Push branch `test`; do not create a Release.
