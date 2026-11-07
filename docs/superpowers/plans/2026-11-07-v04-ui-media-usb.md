# File Manager v0.4 UI, Media and USB Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Deliver the approved v0.4 stability, media, USB, icon and resource-organization update on branch `test`.

**Architecture:** Extend the existing ScePaf pages and settings model without replacing their navigation stack. Port the already-vendored FMVita USB module flow behind a small File Manager service, keep file classification centralized in `core/fs`, and make the RCO the single source of resource paths.

**Tech Stack:** C/C++, VitaSDK, ScePaf RCO, VitaShell/FMVIta kernel-user modules, PowerShell regression checks.

**Spec:** `docs/superpowers/specs/2026-11-07-v04-ui-media-usb-design.md`

## Global Constraints

- Work only on branch `test`; do not create a GitHub Release.
- Preserve every previous VPK and archive a new timestamped v0.4 VPK.
- Do not add `logs/` or `assets/SonyButtons/` to Git.
- Use test-first changes and run the complete build before publishing.

## Review Focus

- Long Music folders must not exhaust memory when cells are recycled.
- Back navigation after search/scroll must restore full-width centered geometry and focus.
- Rename/delete in Preview must update `m_paths` safely and never use a deleted stale entry.
- USB disconnect/failure must restore MTP and unload the kernel module.
- Resource migration must leave no RCO references to missing PNGs.

---

### Task 1: Navigation and Music-folder crash

**Files:** `src/browser_menu.cpp`, `src/browser_menu.h`, `tests/navigation_stability.ps1`

- [ ] Add failing checks for canonical geometry restoration and bounded cell animation.
- [ ] Reset recycled-cell transitions while retaining v0.2 `Popup1` feedback.
- [ ] Restore position/size before return focus and guard `OptionMenu` allocation paths.
- [ ] Parse the supplied dump and document its mapped failure in the test comments.
- [ ] Run all UI regression scripts.

### Task 2: Browser indicators and index icon

**Files:** `resource/file_manager_plugin.xml`, `src/browser_menu.cpp`, `src/ftp_menu.cpp`

- [ ] Add failing layout/state checks for FTP, music and index icons.
- [ ] Position FTP left of `...`, music above it, and bind visibility to actual background state.
- [ ] Replace the `Sup` label with the translucent `search.png` image.
- [ ] Verify RCO compilation.

### Task 3: Image Preview actions and feedback

**Files:** `src/preview_menu.cpp`, `src/preview_menu.h`, `resource/file_manager_plugin.xml`, locale XML files

- [ ] Add failing checks for arrow press opacity, lower filename, Rename and Delete.
- [ ] Add press/release feedback callbacks and center the filename near the lower edge.
- [ ] Add Rename/Delete option handling, confirmation, safe path-vector updates and focus preservation.
- [ ] Run media UI tests.

### Task 4: Music player interaction

**Files:** `src/music_menu.cpp`, `src/music_menu.h`, `resource/file_manager_plugin.xml`

- [ ] Add failing checks for arrow textures, fallback cover, touch seek and seek marker.
- [ ] Use embedded cover when valid and `lg_music.png` otherwise.
- [ ] Make the whole progress track touchable, update time/fill while dragging, and animate the target marker.
- [ ] Verify playback code compiles.

### Task 5: Select and USB service

**Files:** `src/usb.cpp`, `src/usb.h`, `src/settings.*`, `src/settings_menu.*`, `src/browser_menu.*`, `CMakeLists.txt`, module packaging, locale XML files

- [ ] Add failing settings/packaging checks for three Select modes and four USB devices.
- [ ] Port `startUsb`/`stopUsb` behavior and package `usbdevice.skprx`/`umass.skprx`.
- [ ] Persist `usb_device`; show the device collapse when USB is chosen without a configured device.
- [ ] Start the configured device on Select, expose active status, and stop/restore safely.
- [ ] Verify all four device-path mappings.

### Task 6: Icon and file classification

**Files:** `src/core/fs.*`, `src/browser_menu.cpp`, `src/settings_menu.cpp`, RCO and locale files

- [ ] Add failing table-driven checks for every requested extension group and special shortcut.
- [ ] Add a game-file kind without overriding `.suprx` and `.vpk` specialized behavior.
- [ ] Map PSP, Music, favorite folder/file, game, suprx, font, language, theme and lock-state icons.
- [ ] Convert empty Shortcuts to a non-selectable empty state.

### Task 7: Resource organization

**Files:** `resource/ui/**/*.png`, `resource/file_manager_plugin.xml`, `tests/resource_manifest.ps1`

- [ ] Generate a failing resource-reference manifest check.
- [ ] Move all referenced PNGs to `resource/ui` with normalized names.
- [ ] Move unused `resource/ui/New/*.png` files to `resource/ui/backlog`.
- [ ] Update RCO paths and prove every referenced PNG exists and every active PNG is referenced.

### Task 8: Verification and publication

**Files:** timestamped VPK, README if behavior documentation changes

- [ ] Run every PowerShell regression script.
- [ ] Run `git diff --check` and a clean VitaSDK build.
- [ ] Archive the new VPK without replacing earlier packages and record SHA-256.
- [ ] Commit focused changes and push branch `test` only.
