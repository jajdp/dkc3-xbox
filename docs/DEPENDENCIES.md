# Dependencies

*(Español: [DEPENDENCIES.es.md](DEPENDENCIES.es.md))*

Everything here was measured on the machine this port was built on. Where a version matters, it
says why.

## The game project

| | |
|---|---|
| Repository | [elliotttate/DKC3Recomp](https://github.com/elliotttate/DKC3Recomp) |
| Tag | **`v0.0.5`** — the patch is generated against the engine submodule this tag pins |
| Submodule `snesrecomp` | Required. This is the engine, and the patch applies **inside** it: `git submodule update --init --depth 1 snesrecomp` |
| License | MIT for the game project, PolyForm Noncommercial for the engine |

**Keep the path short.** A checkout at something like `C:\dkc3` saves you a build that dies with
`C1083: Cannot open compiler generated file`, which is `MAX_PATH` and not a code problem.

## Your own ROM

| | |
|---|---|
| File | A `.sfc` dump of *Donkey Kong Country 3: Dixie Kong's Double Trouble!* (USA, **headerless**) |
| SHA-256 | `2277a2d8dddb01fe5cb0ae9a0fa225d42b3a11adccaeafa18e3c339b3794a32b` |
| Size | 4,194,304 bytes |

The project verifies the dump itself and refuses anything else, so a different revision will not
get you a build. Nothing about the ROM is distributed here; the hash is a fingerprint.

The recompilation is generated **from your dump**, on your machine, by the project's own script:
about 3,926 `.c` files under `generated/snesrecomp/`. That is why those files are in the
`.gitignore` — they are game code.

## Build tools

| | Version used | Does the version matter? |
|---|---|---|
| Visual Studio 2022 | Community, MSVC 14.44 | **No**, but the *environment* does: `vcvarsall.bat x64 store`, never `vcvars64.bat`. That is what links against the `_app` CRT, which is the one the console has. The C++ **UWP workload is not needed**: the SDK tools do the packaging, not MSBuild |
| Windows SDK | 10.0.26100.0 | **No.** `package_uwp.py` finds the newest SDK that has `makeappx`, `makepri` and `signtool` |
| CMake | 3.20 or newer | Ships with Visual Studio 2022 |
| Ninja | Any | Ships with Visual Studio 2022. ⚠ Its WindowsStore generator does **not** pass `/APPCONTAINER`; `src/CMakeLists.txt` passes it explicitly |
| Python 3 | 3.13 | **No.** Standard library only. 3.8 or newer will do |
| PowerShell | The 5.1 that Windows already ships | **PowerShell 7 is not a dependency.** The examples carry `-ExecutionPolicy Bypass` because a stock Windows is `Restricted` |

**No SDL and no OpenGL.** This port replaces the host layer instead of porting it, so there is no
graphics or input library to find a UWP build of. The two shaders are already compiled to bytecode
in `src/uwp_shaders.h`, so no shader compiler is needed either.

## The console

| | |
|---|---|
| Hardware | Xbox Series X or Series S |
| Mode | **Developer Mode**, with Dev Home installed |
| Access | The Device Portal over your network, and a user signed in on the console — with no user signed in, a package installs and then does nothing when opened |
| Dependency on the console | `Microsoft.VCLibs.140.00`. It is declared in the manifest and installed alongside the package, not bundled inside it |
