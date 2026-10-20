# Donkey Kong Country 3: Dixie Kong's Double Trouble! on Xbox — the UWP port

How to take the [DKC3Recomp](https://github.com/elliotttate/DKC3Recomp) static recompilation of *Donkey Kong Country 3: Dixie Kong's Double Trouble!* (Super Nintendo,
1996) and run it **on an Xbox Series X in Developer Mode**, as a UWP package you build and sign
yourself.

*(Español: [README.es.md](README.es.md))*

It is not a theory: the game boots, plays with a controller at **59.9 frames per second**, with
sound and in 16:9, and the package that runs on two consoles was built with exactly this recipe.

## Which console this was tested on

**An Xbox Series X**, in Developer Mode. Both of the consoles this was built and tested on are Series X,
so that is the only hardware anything claimed here covers.

**A Series S has never been tried — not once.** It is a different machine: less memory available to a
game and a weaker GPU. This may work on one, it may run slower, or it may not start at all; nothing here
has been measured on it. If you run it on a Series S, what you get is not what this README reports, and
none of the numbers here are a promise about that console.

## What this repository is

**A recipe and the tools that apply it.** Nobody had published a UWP build of this game. The port
replaces the game's host layer — the part that opens a window, draws and reads the pad — with one
written for UWP that presents through **Direct3D 11** on the console's `CoreWindow`. It does not
port OpenGL, and it does not use SDL: the engine already produces a finished frame, and this draws
it.

What you apply to your own checkout is one patch of two lines; everything else is new files that sit
beside it.

## What this repository is **not**

It contains **no ROM, no game code, no recompiled sources, no built executable, no `.msix` package
and no artwork**. It cannot produce a playable game on its own, and nothing you can download here
is a game. You need your own legally dumped copy of the cartridge. See [`NOTICE.md`](NOTICE.md).

The package tiles are **not** here either: the ones on the real console were made from the game's
own art, which is Nintendo's. You supply your own — [`docs/ASSETS.md`](docs/ASSETS.md) lists the
exact thirteen files and sizes, and `package_uwp.py` stops with a clear message if one is missing.



## What you need first

| | |
|---|---|
| The game project | [elliotttate/DKC3Recomp](https://github.com/elliotttate/DKC3Recomp) `v0.0.5`, cloned with its `snesrecomp` submodule, **building and running on your PC first** |
| Your own ROM | A `.sfc` dump of *Donkey Kong Country 3: Dixie Kong's Double Trouble!* (USA, headerless). SHA-256 `2277a2d8dddb01fe5cb0ae9a0fa225d42b3a11adccaeafa18e3c339b3794a32b` is the one this was built against, and the project's own ROM check rejects anything else |
| Visual Studio | 2022, C++ tools. What matters is **which environment you build in**: `vcvarsall.bat x64 store`, never `vcvars64.bat` ([`docs/DEPENDENCIES.md`](docs/DEPENDENCIES.md)) |
| Windows SDK | Any recent 10/11 SDK; `package_uwp.py` finds the newest one itself |
| CMake and Ninja | Both ship with Visual Studio 2022 |
| Python 3 | For the patch and the packaging. Standard library only |
| An Xbox in Developer Mode | And a certificate to sign with — a self-signed one is fine ([`docs/INSTALL.md`](docs/INSTALL.md) section 4) |

No SDL, no OpenGL, no shader compiler at run time: the two shaders are compiled to bytecode in
`src/uwp_shaders.h`.

## How it goes

Four steps, and the long version of each one is in [`docs/INSTALL.md`](docs/INSTALL.md).

> **Where to type this:** in **PowerShell** — press the **Windows** key, type `powershell` and press
> **Enter** — in an **ordinary** window: you do not need "run as administrator", and using it would
> break the signing step, because the certificate lives in the store of whoever runs the command.
> From this repository's folder.

```powershell
# 1. patch your checkout (two lines, in the engine submodule; idempotent and checked first)
python tools\apply_uwp.py <path-to-your-checkout>

# 2. generate the recompilation from YOUR rom (this is the project's own script)
#    python scripts\generate_snesrecomp.py --rom "<your rom>.sfc" --analysis-backend native
#    -> about 3,926 .c files under generated/snesrecomp/

# 3. configure and build, in a window where you ran `vcvarsall.bat x64 store`
cmake -S src -B <short-build-path> -G Ninja -DCMAKE_SYSTEM_NAME=WindowsStore ^
      -DCMAKE_SYSTEM_VERSION=10.0 -DCMAKE_BUILD_TYPE=Release ^
      -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL -DDKC3_SRC=<path-to-your-checkout>
cmake --build <short-build-path> --target dkc3_uwp

# 4. package and sign (your own tiles, your own certificate)
python tools\package_uwp.py --exe <short-build-path>\DKC3Recomp.exe ^
       --rom "<your rom>.sfc" --tiles <your tiles folder> ^
       --certificate <your thumbprint> --work <scratch folder>
```

Then install the `.msix` through the Device Portal. Step by step, including the console side:
**[`docs/INSTALL.md`](docs/INSTALL.md)** ([en español](docs/INSTALL.es.md)).

## What it had to solve

Every one of these was a real failure on the console. The detail is in
**[`docs/HOW-IT-WORKS.md`](docs/HOW-IT-WORKS.md)**.

| | Symptom | Fix |
|---|---|---|
| 1 | There is no OpenGL on an Xbox, and SDL's renderer wants an `HWND` UWP does not have | The host layer is replaced, not ported: `src/uwp_host.cpp` presents the finished frame with Direct3D 11 on the `CoreWindow` |
| 2 | The package installed and then would not start (`0x8027025B`) | It imported the **desktop** CRT. Built in the `x64 store` environment it imports the `_app` one, which is what the console has |
| 3 | The console rejected the package on activation | The `APPCONTAINER` bit was missing. Ninja's WindowsStore generator does not add it; `src/CMakeLists.txt` passes `/APPCONTAINER` explicitly |
| 4 | The engine pulled in `user32` | Two `MessageBoxA` calls on a wrong-ROM error. `user32` does not exist inside an AppContainer, and importing it is enough to break activation. They become `OutputDebugStringA` — that is the one patch in `patches/` |
| 5 | `C1083: Cannot open compiler generated file` | The build path was over 260 characters (`MAX_PATH`). Build somewhere short |
| 6 | Pressing **B** closed the game | On an Xbox, B is also the system "back", and the shell takes the app down unless it says it handled the key |
| 7 | Files uploaded as a `.zip` were unreadable by the app | Unpacked by the Device Portal they get permissions the AppContainer cannot open. Upload them one by one |
| 8 | An upload failed with "cannot find the path" | The app's data folder does not exist until its **first run**. Launch it once, close it, then upload |

## How it was verified

Not by reading it: by rebuilding it. On 2026-10-14, from this repository's own `src/` and
`patches/`, against a checkout with the recompilation generated:

| What was measured | Result |
|---|---|
| The patch applies, reverts and is idempotent | Yes; a second run reports `already applied` and writes nothing |
| Does it build? | **3,978 objects, 0 errors**, linking `DKC3Recomp.exe` (9,561,088 bytes) |
| Package and signature | `makepri` → `makeappx` → `signtool`, a signed `.msix` of 9,385,255 bytes |
| Against the `.msix` that runs on the consoles (0.0.4.0) | 10,752 bytes more, 7,168 of them in `.text`: the host has moved on since that package was built |
| Determinism of the toolchain | Measured once for this family rather than three times: the same tree compiled from two different paths gives **93 of 93 objects with an identical `.text`** (see the `supermetroid-recomp-xbox` repository, same engine, same compiler, same flags) |

## Credits and license

Port and tooling by **Recompilaciones**. Released under the
[PolyForm Noncommercial License 1.0.0](LICENSE): the game project itself is MIT, but this port is
built on top of the **snesrecomp** engine, which is PolyForm Noncommercial, and the patch in
`patches/` applies to that engine. Matching the stricter of the two is the honest reading.

*Donkey Kong Country 3: Dixie Kong's Double Trouble!* is © Nintendo. This project is not affiliated with them, with Microsoft, or with the
authors of the game project or the engine, and it distributes nothing that belongs to them. The
details are in [`NOTICE.md`](NOTICE.md). Rights holders can write to **jajdpmail@gmail.com**.
