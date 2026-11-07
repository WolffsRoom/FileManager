# File Manager v0.4 UI, Media, USB and Resource Design

## Intent

Complete the current v0.4 test line without creating a GitHub Release. Preserve the Vita-native behavior while fixing the Music-folder crash, navigation offset, media interaction, Select/USB configuration, icon mapping and resource organization requested by the project owner.

## Chosen approach

Implement the work in independently verifiable layers on branch `test`: stability and navigation first; image/music interactions second; USB and Select configuration third; icon/type classification fourth; resource migration last. Every successful VPK remains timestamped under `scripts/build/vpk/v0.4-test/`.

Alternatives considered:

- A single broad rewrite was rejected because a failure would make the crash, USB and resource-path changes difficult to isolate.
- Keeping aliases in `resource/ui/New` was rejected because the requested final structure is one used `resource/ui` area plus `resource/ui/backlog` for unused PNGs.
- Reusing unbounded v0.2 `Popup1` transitions was rejected because the Music crash dump shows allocation failure entering `OptionMenu`; the visual behavior will be retained with recycled transitions reset safely.

## Behavior

- Folder return restores the prior list to its canonical geometry before it becomes interactive, while keeping the v0.2 3D motion and scroll feedback.
- The Music index button uses the small translucent search icon. Large music folders must not accumulate cell transitions or exhaust memory.
- FTP and minimized-music indicators sit immediately left/above the `...` control and only appear while their services are active in the background.
- Image arrows provide press feedback; the centered filename sits near the bottom edge. Image options add Rename and Delete with confirmation and browser refresh/focus handling.
- The music player uses arrow assets, embedded artwork or `lg_music.png`, touch seeking with immediate visual/time feedback, and an animated seek marker.
- Select supports USB, FTP and QR. USB exposes Memory Card, Game Card, SD2Vita and PSVSD. Choosing USB without a saved device opens the device list; Adjustments persists the selected device and displays it. Active Select/service icons reflect USB, QR or FTP.
- Shortcut and file icons distinguish built-in PSP/Music shortcuts, favorited folders/files, `.suprx`, and game-related extensions.
- The empty shortcuts screen is a proper empty-state message, not a selectable row.
- Font Size, Language, Themes and Auto-decrypt use the supplied assets and state-specific lock icons.
- Used PNGs are referenced from `resource/ui`; unused PNGs from the old `resource/ui/New` set move to `resource/ui/backlog`.

## Constraints

- Keep version `0.4 Test`, branch `test`, and no GitHub Release.
- Preserve all existing timestamped VPKs and the historical `Release` directory.
- USB follows the FMVita/VitaShell implementation and includes the required kernel module in the VPK.
- Destructive preview actions require confirmation and must never operate on a stale path.
- Hardware-only behavior must be reported as requiring PS Vita validation even after build/tests pass.
