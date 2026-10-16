# Changelog

All notable changes to this recipe. Dates are `YYYY-MM-DD`.

The version here is the **recipe's**, not the game's and not the package's. The package version is
whatever you pass to `package_uwp.py --version`.

## 1.0.0 — 2026-10-14

First release. The port itself has been running on two Xbox Series consoles since 2026-09-11; this
is the recipe for it, extracted, cleaned of anything local, and verified by rebuilding.

### Included

- `src/`: the UWP host layer with its Direct3D 11 presenter, the two shaders as source and as
  bytecode, and the CMake target — 1,299 lines, none of them the game's.
- `patches/01-engine-no-user32.patch`: two lines in the engine, which are the only thing in the
  game project this port needs changed.
- `tools/apply_uwp.py` (applies, reverts, lists, checks; idempotent) and `tools/package_uwp.py`
  (`makepri` → `makeappx` → `signtool`, with the five checks that stand between a package that
  installs and a package that starts).
- `package/Package.appxmanifest` with `CHANGE-ME` identity, and documentation in English and
  Spanish.

### Verified before release

Built from this repository's own `src/`: **3,978 objects, 0 errors**, linking `DKC3Recomp.exe`
(9,561,088 bytes), and packaged and signed into a `.msix` of 9,385,255 bytes. The patch applies,
reverts and is idempotent. Detail in [`docs/HOW-IT-WORKS.md`](docs/HOW-IT-WORKS.md).
