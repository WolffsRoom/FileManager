# File Manager v0.6 Settings, Text Search and Selection Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Deliver a v0.6-beta VPK with polished Settings, animated switches, vertical Controls, searchable text preview, persistent marked-row highlighting, and correct Markdown classification.

**Architecture:** Extend the existing ScePaf pages rather than introducing a new UI layer. Settings owns switch animation and font preview state; TextView owns a shared search box and a filtered line-index view; BrowserMenu keeps marked rows visually highlighted through row recycling.

**Tech Stack:** C++11, ScePaf XML/RCO resources, VitaSDK CMake, PowerShell regression tests, PNG assets.

**Spec:** `docs/superpowers/specs/2026-11-08-v06-settings-text-search-selection-design.md`

## Global Constraints

- Version is `v0.6-beta` / `00.60` on branch `test`.
- Preserve the existing ScePaf look, page dimensions, navigation and transitions.
- Preserve every historical VPK; create a uniquely named artifact under `scripts/build/vpk/v0.6-test/`.
- Use the supplied 17 PNG frames per switch direction at 25 ms per frame; fall back to native only if hardware-safe playback cannot be built.
- Controls is a single vertical scrolling list.
- Triangle searches text; Start opens the existing `(...)` edit menu.

## Review Focus

- Rapid repeated switch presses must not overlap animations or leave the displayed state inconsistent with saved settings.
- Empty and no-match text searches must keep the list valid and allow restoring every original line.
- Editing a filtered line must update the corresponding original line rather than the filtered row number.
- Recycled browser rows must retain marked highlighting without making unmarked rows appear selected.
- Long translated control labels and font-preview copy must remain within the 960×544 layout.

---

### Task 1: Version and Markdown classification

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `src/core/fs.cpp`
- Modify: `tests/v05_version.ps1`
- Modify: `tests/icon_resource_coverage.ps1`

**Interfaces:**
- Produces: `VITA_VERSION "00.60"`; `.md` resolves to `fs::Kind_Text` and is absent from `k_gameExtensions`.

- [ ] **Step 1: Write failing assertions** for `00.60`, `.md` text classification and exclusion from game formats.
- [ ] **Step 2: Run** `pwsh -File tests/v05_version.ps1; pwsh -File tests/icon_resource_coverage.ps1` and confirm failures identify the old version and `.md` game rule.
- [ ] **Step 3: Implement** the version bump and remove only `.md` from `k_gameExtensions`.
- [ ] **Step 4: Re-run both tests** and confirm PASS.
- [ ] **Step 5: Commit** `feat: start v0.6 test and fix markdown classification`.

### Task 2: Settings geometry, Themes alignment and font preview

**Files:**
- Modify: `resource/file_manager_plugin.xml`
- Modify: `src/settings_menu.cpp`
- Modify: `src/settings_menu.h`
- Modify: `resource/locale/en.xml`
- Modify: `resource/locale/pt-br.xml`
- Modify: `resource/locale/fr.xml`
- Modify: `resource/locale/es.xml`
- Modify: `resource/locale/de.xml`
- Create: `tests/v06_settings_layout_font_preview.ps1`

**Interfaces:**
- Produces: `SettingsMenu::UpdateFontPreview()` and a Settings-only preview widget; centered theme color overlay; increased neutral gap above the main list.

- [ ] **Step 1: Write the failing layout test** asserting the new list position/height, centered theme preview coordinates, localized preview string, and `UpdateFontPreview()` call after font selection.
- [ ] **Step 2: Run** `pwsh -File tests/v06_settings_layout_font_preview.ps1` and confirm it fails on missing v0.6 layout and preview.
- [ ] **Step 3: Implement** the smallest RCO and SettingsMenu changes, hiding the preview outside `Level_FontSize` and applying the selected font scale immediately.
- [ ] **Step 4: Run the new test plus** `tests/v05_layout_alignment.ps1`, `tests/v05_settings_crash.ps1`, and `tests/v05_settings_visuals.ps1`; confirm PASS.
- [ ] **Step 5: Commit** `feat: refine settings layout and font preview`.

### Task 3: Animated Settings switches

**Files:**
- Modify: `resource/file_manager_plugin.xml`
- Modify: `src/settings_menu.cpp`
- Modify: `src/settings_menu.h`
- Add normalized assets under: `resource/ui/switch/`
- Create: `tests/v06_animated_switch.ps1`

**Interfaces:**
- Produces: `SettingsMenu::StartSwitchAnimation(int32_t row, bool target)` and `SettingsMenu::AdvanceSwitchAnimation()`; terminal textures for on/off state.

- [ ] **Step 1: Write the failing test** asserting 17 registered frames per direction, 96×96 source preservation, a 25 ms step, guarded re-entry and state-save only after the transition starts safely.
- [ ] **Step 2: Run** `pwsh -File tests/v06_animated_switch.ps1` and confirm missing texture/animation failures.
- [ ] **Step 3: Convert/copy the supplied PNG frames mechanically** into stable ASCII paths and register them in the RCO.
- [ ] **Step 4: Implement** the animation state machine, larger switch geometry and final-state refresh without looping.
- [ ] **Step 5: Run** the new test and all Settings tests; confirm PASS.
- [ ] **Step 6: Commit** `feat: animate native settings switches`.

### Task 4: Vertical Controls with normalized Sony assets

**Files:**
- Modify: `resource/file_manager_plugin.xml`
- Modify: `src/settings_menu.cpp`
- Add converted PNG assets under: `resource/ui/controls/`
- Create: `tests/v06_controls_assets.ps1`

**Interfaces:**
- Produces: one scrolling `Level_Commands` list with each source asset normalized into the same visual box.

- [ ] **Step 1: Write the failing test** asserting one-column list layout, required command-to-texture mappings, PNG resources and uniform icon size smaller than ordinary Settings icons.
- [ ] **Step 2: Run** `pwsh -File tests/v06_controls_assets.ps1` and confirm missing normalized resources.
- [ ] **Step 3: Convert the source WebP/PNG controls** to Vita-compatible PNG while preserving aspect ratio and transparent padding.
- [ ] **Step 4: Register and map assets** without altering command order or labels.
- [ ] **Step 5: Run** the new test and navigation/Settings regressions; confirm PASS.
- [ ] **Step 6: Commit** `feat: normalize controls help assets`.

### Task 5: Text preview search and Start options

**Files:**
- Modify: `resource/file_manager_plugin.xml`
- Modify: `src/text_view.cpp`
- Modify: `src/text_view.h`
- Modify: locale files under `resource/locale/`
- Create: `tests/v06_text_search.ps1`

**Interfaces:**
- Produces: `TextView::SetSearchVisible(bool)`, `TextView::ApplySearch(bool)`, `TextView::CloseSearch()` and `m_visibleLines` mapping displayed rows to original `m_lines` indices.

- [ ] **Step 1: Write the failing test** asserting `PAD_TRIANGLE` search, `PAD_MENU` options, incremental filtering, original-line mapping for edit/delete, no-match safety and full-list restoration.
- [ ] **Step 2: Run** `pwsh -File tests/v06_text_search.ps1` and confirm it fails because TextView has no search state.
- [ ] **Step 3: Add the search box/button resources** scoped to `page_text_view` and localized search copy.
- [ ] **Step 4: Implement filtered index mapping** and update CreateLine, RefreshRow, editing, deleting and jump operations to resolve original indices.
- [ ] **Step 5: Implement control mapping** so Triangle toggles search and Start opens the existing option menu.
- [ ] **Step 6: Run** the new test, `tests/preview_actions.ps1` and the full text/preview regressions; confirm PASS.
- [ ] **Step 7: Commit** `feat: add incremental text preview search`.

### Task 6: Persistent marked-row highlight

**Files:**
- Modify: `src/browser_menu.cpp`
- Modify: `src/browser_menu.h`
- Modify: `resource/file_manager_plugin.xml` if a dedicated persistent plane is required
- Modify: `tests/v05_native_selection_mark.ps1`
- Create: `tests/v06_persistent_mark_highlight.ps1`

**Interfaces:**
- Produces: one row-decoration helper that applies marked state during creation, focus changes, toggles and recycled-row refresh.

- [ ] **Step 1: Write the failing test** asserting marked rows use the cursor-equivalent colored highlight after focus leaves, while unmarked recycled rows reset to neutral.
- [ ] **Step 2: Run** `pwsh -File tests/v06_persistent_mark_highlight.ps1` and confirm the current focus-only behavior fails.
- [ ] **Step 3: Implement** the minimal shared row-state update and call it from row creation, mark toggle, clear/select-all and focus callbacks.
- [ ] **Step 4: Run** selection, navigation and recycling regressions; confirm PASS.
- [ ] **Step 5: Commit** `fix: keep marked browser rows highlighted`.

### Task 7: Full verification and v0.6-beta package

**Files:**
- Create: `docs/v0.6-test-settings-text-search-selection.md`
- Create: unique VPK under `scripts/build/vpk/v0.6-test/`

**Interfaces:**
- Consumes: all preceding behavior.
- Produces: verified v0.6-beta artifact and hardware validation checklist.

- [ ] **Step 1: Run every** `tests/*.ps1` script and record the exact pass/fail count.
- [ ] **Step 2: Run the Release build** with the repository build script and confirm success.
- [ ] **Step 3: Validate** VPK ZIP entries, `param.sfo` version, byte size and SHA-256.
- [ ] **Step 4: Write release-test notes** covering every requested change and PS Vita physical checks.
- [ ] **Step 5: Commit** `build: archive v0.6 settings and search test`.
