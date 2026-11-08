# v0.5 Native Navigation and Layout Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restore original native folder behavior, correct Settings/selection layout, and organize local VPK history safely.

**Architecture:** Preserve asynchronous filesystem loading and existing page ownership. Isolate search geometry changes from page transitions, use only the original ScePaf transition calls for standard navigation, and encode the visual layout in the RCO.

**Tech Stack:** C++11, ScePaf, RCO XML, PowerShell regression checks, VitaSDK.

**Spec:** `docs/superpowers/specs/2026-11-08-v05-native-navigation-layout-design.md`

## Global Constraints

- Never delete or overwrite an existing VPK.
- Keep work on branch `test` and do not create a Release.
- Preserve main-thread PAF safety introduced after the initial release.

## Review Focus

- Closing an already-hidden search must not recenter or reposition the list.
- Back during/after a native transition must restore the parent without a second geometry mutation.
- Search opening and closing must still resize the active list correctly.
- Long translated selection labels must remain centered and ellipsized.
- Version organization must preserve byte hashes of moved historical VPKs.

---

### Task 1: Native navigation regression

**Files:**
- Modify: `src/browser_menu.cpp`
- Modify: `src/browser_menu.h`
- Create: `tests/v05_native_navigation_geometry.ps1`

**Interfaces:**
- Consumes: `SetSearchVisible(bool)`, `UpdateChrome()`, `PushPage()`, `PopPage()`.
- Produces: idempotent search visibility changes that cannot mutate an unrelated transition.

- [ ] Write a failing regression covering hidden-search idempotence and the initial ScePaf transition sequence.
- [ ] Run it and confirm failure against the unconditional geometry/focus reset.
- [ ] Make `SetSearchVisible` return without geometry or focus changes when state is unchanged; keep explicit initialization for new pages.
- [ ] Remove any redundant foreground reset absent from the initial lifecycle.
- [ ] Run navigation regressions and compile.
- [ ] Commit the navigation fix.

### Task 2: Settings and selection layout

**Files:**
- Modify: `resource/file_manager_plugin.xml`
- Create: `tests/v05_layout_alignment.ps1`

**Interfaces:**
- Produces: transparent power action style below the header and symmetric selection-button centers.

- [ ] Write a failing layout regression using parsed RCO coordinates/styles.
- [ ] Confirm it fails on opaque/high Settings buttons and current selection centers.
- [ ] Adjust Settings power style/vertical position and list area.
- [ ] Place the four selection actions symmetrically inside the area left of `...`.
- [ ] Compile the RCO and run UI regressions.
- [ ] Commit the layout fix.

### Task 3: Local VPK organization

**Files:**
- Create: `scripts/build/vpk/legacy/README.md`
- Move: historical loose VPKs from `build/` and `Release/` into versioned legacy directories.
- Move: non-final v0.5 test artifacts into `scripts/build/vpk/v0.5-test/archive/`.

**Interfaces:**
- Produces: versioned local history with unchanged SHA-256 hashes.

- [ ] Record source paths and hashes before moving.
- [ ] Move only historical VPKs; preserve the active `build/FileManager.vpk` output path.
- [ ] Verify counts and hashes after moving.
- [ ] Document the directory convention.
- [ ] Commit only repository-appropriate organization metadata/artifacts.

### Task 4: Final package and branch update

**Files:**
- Create: `docs/v0.5-test-native-navigation-layout.md`
- Create: a uniquely named VPK under `scripts/build/vpk/v0.5-test/`.

**Interfaces:**
- Consumes: Tasks 1-3.
- Produces: validated v0.5 test VPK on branch `test`.

- [ ] Run every PowerShell test.
- [ ] Build Release v0.5-test.
- [ ] Validate VPK ZIP entries and SHA-256.
- [ ] Commit documentation and the new VPK without replacing prior packages.
- [ ] Push branch `test`; do not create a Release.
