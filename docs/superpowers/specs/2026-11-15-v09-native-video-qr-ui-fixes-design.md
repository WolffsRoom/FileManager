# v0.9 Native Video, QR Download, and UI Corrections Design

## Intent

Finish the v0.9 media and navigation work on real PlayStation Vita hardware. MP4 playback must show video and play audio without fighting a minimized MP3. QR codes that point directly to downloadable files, including GitHub release assets, must download safely instead of showing a network error or crashing. The related browser, FTP, selection, and settings defects reported in the same hardware test pass are included because they share the affected pages and input lifecycle.

## Goals

- Render local H.264/AAC MP4 files through `SceAvPlayer` in the existing PAF preview page, with visible frames, working sound, progress, pause, seek, and clean close/reopen behavior.
- Add video options for playback speed cycling through `1x`, `1,5x`, and `2x`, plus `Exportar` to the native Videos library.
- Suspend an active background MP3 before video playback and restore its previous paused/playing state when the video closes.
- Make Circle close the search/keyboard state before it is allowed to navigate to the parent folder; reopening search must not displace the current list.
- Decode the supplied QR code without overflowing the PAF callback stack, follow GitHub release redirects, show download progress, and save the VPK under `ux0:Downloads` without crashing after an error dialog.
- Display an FTP client's hostname when reverse lookup succeeds and a neutral private client label when it does not; never expose the client IP in the UI.
- Make Select All a true toggle with `Selec. Tudo` / `Desel. Tudo`, repaint rows immediately, and make the selection bar exactly 960 pixels wide.
- Map Triangle to the quick options menu and Start to search on the browser page.
- Remove stale browser-selection tint from Settings rows.
- Reduce font choices to `Pequeno` and `Maior`, default new installs to `Pequeno`, map the old Normal size to `Maior`, and migrate the removed Large value to `Maior`.
- Move font choices upward and provide a centered two-part preview containing the existing small sentence plus an emphasized `Lorem Ipsum` introduction and the requested paragraph.
- Rename `Transparência dos previews` to `Opacidade dos menus` in Brazilian Portuguese and update equivalent locale wording consistently.

## Non-goals

- Adding new video codecs or software decoding beyond the MP4/H.264/AAC path supported by the Vita hardware.
- Streaming video directly from a QR URL; QR media remains download-first.
- Displaying a guessed or fabricated FTP hostname when reverse DNS is unavailable.
- Changing the visual theme, image preview behavior, music waveform design, or unrelated v0.9 features.

## Constraints and anti-goals

- Hardware behavior is authoritative; Vita3K alone cannot validate `SceAvPlayer`, audio ports, camera QR scanning, or Vita TLS behavior.
- A valid pointer-like `SceAvPlayerHandle` must never be rejected merely because its signed integer representation is negative.
- Decoder-owned frame memory must not be freed or reused while PAF/GXM can still consume it.
- No second active application audio pipeline may run while video owns the main audio output.
- Search close and page navigation must be separate state transitions; a single Circle action must not perform both.
- Network failures and malformed QR frames must return to a usable browser page, never leave background jobs targeting destroyed UI objects, and never crash while dismissing a dialog.
- Existing user files, logs, assets, and unrelated untracked workspace content remain untouched.

## Approaches considered

### 1. NetStream RGBA player plus libcurl/OpenSSL — selected

Package and load the RGBA `SceAvPlayer` support module used by NetStream, follow its PAF surface path, and use the proven libcurl/OpenSSL download stack used by Wolff's Store for modern GitHub TLS and redirects. This is the most direct fit for the application's PAF renderer and the most reliable option on hardware. It increases the VPK size and adds a packaged module, but avoids per-frame CPU color conversion and preserves smooth PAF overlays.

### 2. CPU YUV-to-RGBA conversion

Keep the existing player and convert every decoded NV12 frame into an owned PAF RGBA surface. This changes fewer dependencies but spends substantial CPU and memory bandwidth per frame, making dropped frames and audio desynchronization more likely on the Vita.

### 3. Separate direct-GXM renderer

Port the Wolff's Store/Vita2D YUV texture path and render outside the normal PAF surface flow. It is efficient but creates a second presentation path, complicates controls and overlays, and raises lifecycle risk when returning to PAF pages.

## Design

### Native video lifecycle

`VideoPlayer` will own a single explicit state machine: closed, preparing, ready/playing, paused, ended, and failed. It will load the packaged RGBA AvPlayer module once, initialize `SceAvPlayer` with an event callback and `autoStart = false`, add the local source, wait for the ready state, enable one video and one audio stream, start the audio worker, and call `sceAvPlayerStart`.

Video frames will come from the RGBA-capable AvPlayer path and be wrapped as non-owning PAF `ImageMode_RGBA8888` surfaces using the decoder-provided pitch. Surface wrappers will be cached per decoder frame pointer. Closing will first unregister the preview update, clear the plane texture, synchronize rendering, stop and join audio, stop/close AvPlayer, release wrappers, and unload only resources owned by the player.

The first-frame and first-audio events, stream properties, speed changes, errors, and shutdown stages remain in `ux0:data/FileManager/video_diagnostic.txt` for real-hardware diagnosis.

### Audio ownership and speed

The music core will expose explicit pause/resume state helpers instead of relying on a blind toggle. Starting a video records whether music was open and playing, pauses it, and releases or suspends conflicting output ownership before opening the video output. Closing the video restores playback only if it had been playing before the video opened.

The options menu displays the current speed and cycles `1x -> 1,5x -> 2x -> 1x`. It calls `sceAvPlayerSetTrickSpeed` with 100, 150, and 200. If a firmware rejects 150, the option returns to 1x, records the exact error, and shows a nonfatal error rather than simulating speed with repeated seeks.

`Exportar` uses the existing `mediaExport` path for MP4 and its current progress/cancel dialog, then reports completion without closing the preview unexpectedly.

### QR decoding and GitHub downloads

The large `quirc_code` and `quirc_data` work buffers move off the PAF callback stack into owned heap storage. Each frame validates decoder dimensions, QR count, extracted grid size, and payload length before decoding or copying. Once a valid payload is accepted, camera updates are stopped before the result callback is queued.

HTTP inspection and download move to a C++ libcurl/OpenSSL wrapper with redirect following, response status validation, content-disposition filename parsing, a `.part` destination, cancellation, progress callbacks, and atomic rename. The wrapper uses a bounded redirect count and removes partial files on failure. GitHub's `releases/latest/download/...` redirect chain is treated like any other direct file response. The browser UI remains alive until the worker posts its terminal result.

### Search and control mapping

`OnBack` first checks `m_searchVisible`. If true, it ends text editing, clears the filter, restores the current list geometry, and consumes the action. Only a later Circle action can request parent navigation. Search geometry is attached to the active `BrowserPage` state rather than only the global menu flag, so a page transition cannot reuse another page's compact-search dimensions.

Start owns search. Triangle and the lower-right corner button own the quick options menu. Existing L/R, Square, Select, and Circle behavior remains unchanged.

### Selection toolbar

The selection toolbar is 960 pixels wide. The Select All control is retained as a widget pointer so its label can be refreshed whenever selection changes. If every selectable entry is marked, it shows `Desel. Tudo`; otherwise it shows `Selec. Tudo`. Activating it sets all rows to the opposite aggregate state and repaints every instantiated row immediately using the existing UI selection color. Recycled rows still derive their color from the entry model when recreated.

### FTP client privacy

When ftpvita reports a connection IP, the service performs a bounded reverse-DNS lookup outside the UI callback. A successful hostname is stored in `Status.client`. If lookup is unavailable or times out, the UI shows a localized neutral value such as `Cliente conectado`, never the raw IP. Transfer activity and server address behavior are otherwise unchanged.

### Settings and font preview

Settings rows explicitly hide the browser-only selection plane whenever a row is created or recycled. Their normal focus treatment remains native, while saved choices continue to use switches only.

Font settings expose two rows. `Pequeno` remains 22 px and becomes the fresh-install default. `Maior` uses the former Normal size of 28 px. Persisted values 1 and 2 both migrate to `Maior`; value 0 stays `Pequeno`. The list occupies the upper portion of the page. The centered lower preview uses separate PAF text widgets so `Lorem Ipsum` can be emphasized without making the remaining paragraph bold.

The opacity setting keeps its stored key for backward compatibility but its displayed name becomes `Opacidade dos menus`.

## Error handling

- Video init, module load, stream, output, speed, and export failures are reported without invalidating the preview page.
- QR decoder rejection simply continues scanning; allocation or camera failure closes cleanly with one error.
- Download failures close progress UI exactly once, delete `.part`, and leave the browser interactive.
- Reverse-DNS failure degrades to a generic client label.
- Removed font values are normalized during load before any locale array indexing.

## Verification

Automated regression tests will cover state and source invariants for video lifecycle, audio ownership, speed choices, export routing, QR heap buffers and validation, redirect/download cleanup, search back handling, control mapping, select-all toggling and labels, toolbar width, settings selection-plane reset, font migration/layout/copy, and locale renaming.

The VitaSDK build and VPK packaging must succeed with the RGBA module and curl/OpenSSL dependencies included. Final hardware checks require the user's Vita for visible MP4 frames, audible/synchronized AAC, MP3 restoration, 1x/1.5x/2x behavior, video export, supplied QR download, FTP hostname display, and touch/controller navigation.

## Files expected to change

- `src/video_player.cpp`, `src/video_player.h`, `src/preview_menu.cpp`, `src/preview_menu.h`
- `src/core/music.c`, `src/core/music.h`, `src/core/media_export.c`
- `src/core/download.c` or a replacement `src/core/download.cpp`, plus its header
- `src/qr_menu.cpp`, `src/qr_menu.h`
- `src/browser_menu.cpp`, `src/browser_menu.h`
- `src/ftp.cpp`, `src/ftp.h`, `src/ftp_menu.cpp`
- `src/settings.cpp`, `src/settings.h`, `src/settings_menu.cpp`, `src/settings_menu.h`
- `resource/file_manager_plugin.xml`, locale XML files, and generated resource inputs
- `CMakeLists.txt`, VPK packaging inputs, and regression tests under `tests/`
