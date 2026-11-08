# Local VPK archive

This directory preserves historical packages that previously existed as loose files in `build/` and `Release/`.

## Layout

- `alpha/build/` - alpha packages formerly stored in `build/`.
- `alpha/release/` - alpha package formerly stored in `Release/`.
- `beta/build/` and `beta/release/` - beta packages grouped by their original location.
- `v0.3/build/` and `v0.3/release/` - v0.3/v0.30 beta packages grouped by their original location.
- `../v0.5-test/archive/` - intermediate v0.5 test builds.

The current compiler output remains at `build/FileManager.vpk`. Published test packages remain directly inside their version directory. Builds must always receive a unique timestamped name and must never overwrite an older VPK.
