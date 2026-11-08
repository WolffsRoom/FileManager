# File Manager v0.5 Native Navigation, Selection and Search Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix the Settings crash and restore alpha-VPK navigation while refining selection and incremental search for v0.5.

**Architecture:** Keep the current worker/UI-thread separation, but return page transitions to the alpha ScePaf sequence and remove frame-count geometry ownership. Centralize checkbox lookup at the list-item level and separate incremental filtering from keyboard completion.

**Tech Stack:** C++, ScePaf, VitaSDK, PowerShell structural regressions.

**Spec:** `docs/superpowers/specs/2026-11-08-v05-native-navigation-selection-search-design.md`

## Global Constraints

- `scripts/build/vpk/legacy/alpha/build/FileManager (alpha).vpk` is the navigation/scroll reference.
- Labels are `Padrão` and `Simplificado` in Portuguese.
- Preserve every prior VPK; new artifacts go under `scripts/build/vpk/v0.5-test/`.
- Work on branch `test`; do not create a GitHub Release.
- Do not commit `logs/` or `assets/SonyButtons/`.

## Review Focus

- Empty Settings rows or recycled placeholders must not dereference a missing checkbox.
- Back during an active transition must not free a ListView still used by ScePaf.
- Search string-change callbacks must not close the IME or recursively trigger filtering.
- Selection state must remain correct after rows are recycled during long-folder scrolling.
- The options corner must remain touchable while the selection toolbar is visible.

---

### Task 1: Settings crash and navigation labels

**Files:**
- Modify: `src/settings_menu.cpp`
- Modify: `resource/locale/*.xml`
- Create: `tests/v05_settings_crash.ps1`

**Interfaces:**
- Produces: safe `SettingsMenu::CreateItem` checkbox lookup from the list item and v0.5 navigation labels.

- [ ] Write a regression that validates sibling checkbox lookup, null guards, the supplied crash mapping, and `Padrão`/`Simplificado` labels.
- [ ] Run it and observe failure on the button-relative lookup.
- [ ] Resolve the checkbox from `item`, guard optional widgets, and update all locales.
- [ ] Run the new test and existing Settings regressions.
- [ ] Commit the task.

### Task 2: Alpha-native folder navigation

**Files:**
- Modify: `src/browser_menu.cpp`
- Modify: `src/browser_menu.h`
- Create: `tests/v05_alpha_navigation.ps1`

**Interfaces:**
- Consumes: current main-thread `CommitLoad` and deferred input callbacks.
- Produces: native `PushPage`/`PopPage` transition lifecycle without frame-count geometry correction.

- [ ] Write a regression comparing the transition calls to commit `837135e` and rejecting `m_transitionFrames` geometry ownership.
- [ ] Run it and observe failure on the current 20/24-frame controller.
- [ ] Restore `Type_3D_SlideToBack1` and reverse `Type_3D_SlideFromFront`; retain pages until ScePaf completion through a transition callback/deferred retirement.
- [ ] Ensure parent focus/scroll is preserved and simplified mode remains a fade.
- [ ] Run navigation and long-list regressions, then commit.

### Task 3: Selection toolbar and checkbox consistency

**Files:**
- Modify: `resource/file_manager_plugin.xml`
- Modify: `src/browser_menu.cpp`
- Modify: `src/browser_menu.h`
- Create: `tests/v05_selection_layout.ps1`

**Interfaces:**
- Consumes: the sibling checkbox hierarchy fixed in Task 1.
- Produces: `BrowserMenu::RowCheckbox(ListItem*)` and a toolbar that leaves Options unobstructed.

- [ ] Write failing layout and row-recycling regressions for narrower buttons, uncovered Options corner, and shared checkbox lookup.
- [ ] Run them and observe failure on the full-width surface/duplicated lookup paths.
- [ ] Add the shared lookup helper and route create/toggle/select-all/clear through it.
- [ ] Resize/reposition the toolbar and four buttons so `...` remains in front and touchable.
- [ ] Run selection regressions and commit.

### Task 4: Incremental search and v0.5 artifact

**Files:**
- Modify: `src/browser_menu.cpp`
- Modify: `src/browser_menu.h`
- Create: `tests/v05_incremental_search.ps1`
- Create: `docs/v0.5-test-native-navigation.md`

**Interfaces:**
- Produces: `FilterSearch(bool finishEditing)` used by string-change and Enter callbacks.

- [ ] Write failing tests for head focus before IME, `CB_TEXT_BOX_STRING_CHANGED`, live filtering without `EndEdit`, and Enter completion.
- [ ] Run and observe failure on Enter-only filtering.
- [ ] Split filtering from editing completion and defer string-change list mutation to the main UI frame.
- [ ] Run every PowerShell regression and `git diff --check`.
- [ ] Build Release as `v0.5-test`, verify VPK integrity/hash, document hardware checks, commit, and push branch `test` only.
