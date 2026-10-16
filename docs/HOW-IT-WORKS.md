# How it works

*(Español: [HOW-IT-WORKS.es.md](HOW-IT-WORKS.es.md))*

## The decision that defines this port

A UWP process on an Xbox cannot use OpenGL, and SDL's Direct3D renderer asks for an `HWND` that
UWP does not have. The game's desktop host is built on OpenGL.

So the host is **not ported — it is replaced**. The engine already produces a finished frame in
memory; `src/uwp_host.cpp` is a host layer that opens the `CoreWindow`, uploads that frame as a
Direct3D 11 texture, draws it with two triangles, reads the gamepad through
`Windows.Gaming.Input`, and plays the audio the engine hands it. 1,299 lines, and not one
line of the game's own code.

That is also why there is almost nothing in `patches/`: when you replace the host instead of
patching it, the only thing left to change in the game project is the two calls that drag in
`user32`.

## The three things the console refuses

1. **`user32`.** `MessageBoxA` lives there, the engine calls it twice on a wrong-ROM error, and
   inside an AppContainer that library does not exist. A package that merely *imports* it installs
   and then fails to activate. The patch turns both calls into `OutputDebugStringA`, which still
   reaches a debugger and the console log.
2. **The desktop CRT.** Built in the ordinary `vcvars64` environment the executable imports
   `MSVCP140.dll`; the console has the `_app` variant instead. Same symptom: installs, will not
   start, `0x8027025B`. `vcvarsall.bat x64 store` is the whole fix, and `package_uwp.py` refuses to
   package an executable without the `_app` CRT.
3. **The `APPCONTAINER` bit.** Ninja's WindowsStore generator does not set it, and without it the
   loader rejects the package when it tries to activate. `src/CMakeLists.txt` passes
   `/APPCONTAINER` explicitly.

## B is not a button

On an Xbox, **B** is the system "back". If the app does not mark the key event as handled, the
shell takes it down — the game appears to crash when you press B. The host handles `GamepadB`,
`GoBack` and `Escape` in `CoreWindow::KeyDown` and marks them handled. It is two lines, and it is
mandatory in any UWP host for a console.

## Packaging the wrong executable

The one failure here that is not technical. `package_uwp.py` accepts any `--exe` that passes its
checks, and those checks cannot tell a current build from a stale one sitting in another build
tree. That is how the package installed on the consoles (0.0.4.0) ended up **without the save
code that the source already had**: it was built from an older executable.

Measured, 2026-10-14: the executable this recipe builds is 10,752 bytes more, 7,168 of them in `.text`: the host has moved on since that package was built. The strings
`%s/save.srm`, `Failed fopen for save: %s` and the whole `Suspending` handler are in it and absent
from the installed package.

The lesson is cheap to apply: package straight out of the build you just ran, and if a feature you
know is in the source is missing on the console, suspect the executable before suspecting the code.

## Why MAX_PATH matters here more than usual

Every bank of the game is a separate translation unit, so this is a 3,978-object build. The
object paths mirror the source paths, and a checkout a few folders deep is enough for the compiler
to fail with `C1083: Cannot open compiler generated file` — which reads like a broken toolchain and
is just a long path.

## How this was verified

Not by reading it: by rebuilding it. On 2026-10-14, from this repository's own `src/` and
`patches/`, against a checkout with the recompilation generated:

| What was measured | Result |
|---|---|
| The patch applies, reverts and is idempotent | Yes; a second run reports `already applied` and writes nothing |
| Does it build? | **3,978 objects, 0 errors**, linking `DKC3Recomp.exe` (9,561,088 bytes) |
| Package and signature | `makepri` → `makeappx` → `signtool`, a signed `.msix` of 9,385,255 bytes |
| Against the `.msix` that runs on the consoles (0.0.4.0) | 10,752 bytes more, 7,168 of them in `.text`: the host has moved on since that package was built |
| Determinism of the toolchain | Measured once for this family rather than three times: the same tree compiled from two different paths gives **93 of 93 objects with an identical `.text`** (see the `supermetroid-recomp-xbox` repository, same engine, same compiler, same flags) |

## A note on language

The host's own comments and its trace strings are in Spanish, which is where they were written.
They are published as they were compiled and verified, rather than retyped for publication; what
the scripts print, and all of this documentation, is in English and Spanish.
