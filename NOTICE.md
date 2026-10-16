# Notice

*(Español: [NOTICE.es.md](NOTICE.es.md))*

## Not affiliated

This is an unofficial, noncommercial fan project. It is **not affiliated with, endorsed by or
connected to Nintendo or Microsoft**, nor to the authors of [elliotttate/DKC3Recomp](https://github.com/elliotttate/DKC3Recomp) or
[snesrecomp](https://github.com/mstan/snesrecomp). *Donkey Kong Country 3: Dixie Kong's Double Trouble!*, *Xbox* and all related names and
marks are the property of their respective owners.

Nothing here is for sale, and nothing here is monetized.

## What this repository does not distribute

No ROM. No game code of Nintendo's. No recompiled sources — those are produced on your machine,
from your dump, by the project's own script. No built executable. No `.msix` package. No save
data. No package tiles, which were made from the game's own artwork. No signing certificate, and
no thumbprint of one.

The `.gitignore` blocks all of it by extension. That is a safeguard, not a substitute for reading
this file.

## What it does contain of other people's work

Measured, so that "we distribute nothing of theirs" is something you can check rather than
something you have to believe:

| | Lines | What it is |
|---|---|---|
| Our own code | 1,299 | `src/uwp_host.cpp`, `src/uwp_shaders.h`, `src/uwp_shaders.hlsl` and `src/CMakeLists.txt`: the UWP host layer, the Direct3D 11 presenter and the build target. Written here |
| Our own tooling | 459 | `tools/apply_uwp.py` and `tools/package_uwp.py` |
| Our own lines inside the patch | 2 | Two `OutputDebugStringA` calls replacing two `MessageBoxA` calls |
| Quoted from snesrecomp | 12 | The **context lines** a unified diff has to carry to know where the change goes, plus the 2 lines it removes. Nothing else of theirs is here |

The engine those 14 lines come from is licensed under the
[PolyForm Noncommercial License 1.0.0](https://polyformproject.org/licenses/noncommercial/1.0.0),
which permits changes and new works for noncommercial purposes, and this repository is released
under that same license to match. The game project is MIT, and no line of it is reproduced here at
all — the host layer this port replaces is ours.

The ROM is yours to supply and never travels through here. What the recipe carries about it is a
**SHA-256 hash** — a fingerprint, not content.

## Takedown

If you hold rights to any of the above and want something removed, write to
**jajdpmail@gmail.com** and it will be taken down. No argument, no delay.
