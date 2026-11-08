# v0.5 Native Navigation and Layout Design

## Intent

Restore the initial File Manager navigation feel while keeping the later safety work, correct the Settings and selection layouts, and organize local VPK history without deleting any build.

## Chosen approach

Use the original ScePaf transition lifecycle from commit `837135e` as the behavioral reference. Keep filesystem I/O off the UI thread, but stop search chrome updates from mutating list geometry or focus during folder transitions. Do not restore the unsafe original worker-thread UI mutations and do not recreate parent lists.

## Requirements

- **Padrão** uses the original ScePaf forward, reverse, and scrolling behavior.
- Returning to a parent preserves its horizontal center, focused item, and scroll state.
- Search geometry changes only when search visibility actually changes.
- Settings power actions are transparent and sit below the `Ajustes` header.
- Selection actions are evenly spaced and centered in their available lower bar while `...` remains visible.
- Historical VPKs are grouped by version/status. No VPK is deleted or overwritten.
- The branch remains `test`; no GitHub Release is created.

## Alternatives considered

- Full source rollback: rejected because it restores unsafe PAF operations from the filesystem worker.
- Recreate parent lists on return: rejected because it loses native transition and scroll state.
- Preserve the current manual geometry resets: rejected because they are the likely source of intermittent half-screen return state.
