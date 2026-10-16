# Install — from a clean checkout to the game running on the console

*(Español: [INSTALL.es.md](INSTALL.es.md))*

Eight sections. Sections 1 to 5 happen on your PC, 6 and 7 on the console. Where a step has a trap,
the trap is in the step and not in a footnote.

## 0. Where you type these commands

- **PowerShell** — press the **Windows** key, type `powershell`, press **Enter**. For the Python
  scripts, the certificate and the Device Portal.
- **A `cmd` window with the build environment loaded** — press **Windows**, type `cmd`, **Enter**,
  then run the `vcvarsall.bat` line from section 3. For CMake and Ninja. The environment lasts only
  as long as that window.

**You do not need "run as administrator"**, and using it would break section 4: the certificate is
created in the store of whoever runs the command, so an elevated window puts it where the signing
step cannot find it. The `-ExecutionPolicy Bypass` in the examples is because a stock Windows
refuses to run `.ps1` files at all; it applies to that one process and changes nothing on your
system.

## 1. Clone the game project

```powershell
git clone --branch v0.0.5 https://github.com/elliotttate/DKC3Recomp.git C:\dkc3
cd C:\dkc3
git submodule update --init --depth 1 snesrecomp
```

**Keep the path short** (`C:\dkc3`, not a folder deep inside Documents). A long one makes the
build die with `C1083: Cannot open compiler generated file`, which is `MAX_PATH` and looks like a
code error.

## 2. Patch, then generate the recompilation

The patch first, because it is two lines and it is quick:

```powershell
python tools\apply_uwp.py C:\dkc3
```

It checks before it writes, it is idempotent, and `--list` explains what it does: the engine's two
`MessageBoxA` calls become `OutputDebugStringA`, because `user32` does not exist inside an
AppContainer and merely importing it is enough to make the package install and then fail to
activate.

Then the recompilation, **from your own ROM**, with the project's own script:

```powershell
python scripts\generate_snesrecomp.py --rom "<your rom>.sfc" --analysis-backend native
```

It writes about 3,926 `.c` files into `generated/snesrecomp/`. Those are the game, translated
to C — that is why this repository does not and cannot ship them.

## 3. Build

In a **`cmd` window**:

```
"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 store
```

```
cmake -S "<this repo>\src" -B C:\dkc3b -G Ninja ^
  -DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION=10.0 ^
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL ^
  -DDKC3_SRC=C:/dkc3

cmake --build C:\dkc3b --target dkc3_uwp
```

- **`x64 store`, not `vcvars64`.** That is what links the `_app` CRT, which is the one the console
  has. With the desktop CRT the package installs and the console then says *"the app didn't
  start"* (`0x8027025B`).
- **`-DDKC3_SRC` takes forward slashes**, even on Windows: with backslashes CMake reads them as
  escapes.
- It is a big build — here it was **3,978 objects** — because every bank of the game is a
  translation unit.
- The output is `C:\dkc3b\DKC3Recomp.exe`.

## 4. A certificate to sign with

One command, in **PowerShell**, in an **ordinary** window:

```powershell
New-SelfSignedCertificate -Type Custom -Subject "CN=<your name>" -KeyUsage DigitalSignature `
  -FriendlyName "<your name> UWP dev" -CertStoreLocation "Cert:\CurrentUser\My" `
  -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3", "2.5.29.19={text}")
```

Write down the `Thumbprint`, then open [`../package/Package.appxmanifest`](../package/Package.appxmanifest)
and replace the two `CHANGE-ME` values. `Publisher` must be **exactly** the `-Subject` above, `CN=`
included: the console compares the two strings and rejects the package if they differ.
`Name` plus `Publisher` form the family the console identifies the game by, and the `LocalState`
with your saves hangs off it — pick it once and leave it.

## 5. Package, sign, and install

Your own tiles first ([`ASSETS.md`](ASSETS.md) lists the thirteen and their sizes; this repository
ships none, because the ones on the console were made from the game's art).

```powershell
python tools\package_uwp.py --exe C:\dkc3b\DKC3Recomp.exe ^
  --rom "<your rom>.sfc" --tiles <your tiles folder> ^
  --certificate <your thumbprint> --version 0.1.0.0 --work C:\dkc3pkg
```

Before packaging it checks the five things that make a package install and then not start: the
`APPCONTAINER` bit, the `_app` CRT, every imported DLL, the Publisher against the certificate, and
the thirteen tiles. Here that produced a signed `.msix` of about 9,385,255 bytes.

Install it from the console's **Device Portal** (`https://<console ip>:11443`, with your Dev
Home username and password): **Add** → the `.msix` → and, as a dependency, the
`Microsoft.VCLibs.140.00` appx for x64 from the Windows SDK. The console needs **a user signed
in**; with nobody signed in the package installs and then does nothing when opened.

⚠ Right after installing, launching can fail with *"Failed to launch the application"*. The
portal's package list caches the old and the new version together for a moment, and a system modal
on screen does the same. Wait, dismiss anything on the TV, and try again.

## 6. Uploading files to the app

Two things that cost an afternoon each:

- The app's data folder **does not exist until its first run**. Launch the game once, close it, and
  only then upload anything — otherwise the upload fails with "cannot find the path".
- **Upload files one by one, not as a `.zip`.** When the Device Portal unpacks an archive it leaves
  the files with permissions the AppContainer cannot open, and the app behaves as if they were not
  there.

## 7. Playing it

Connect the controller **before** launching. **B** works as you would expect inside the game: the
host marks it as handled, which is what stops the console's shell from closing the app.

## 8. When something goes wrong

| What you see | What it is |
|---|---|
| `C1083: Cannot open compiler generated file` | The checkout or the build path is too long (`MAX_PATH`) |
| *"the app didn't start"* / `0x8027025B` | The desktop CRT (built with `vcvars64`), or a DLL missing from the package. `package_uwp.py` catches both before signing |
| The console rejects the package on install | `Publisher` and the certificate subject are not the same string (section 4) |
| The console rejects it on activation | The `APPCONTAINER` bit is missing — a build that did not come from `src/CMakeLists.txt` |
| It rejects your ROM | It is not the exact USA headerless dump. The expected SHA-256 is in [`DEPENDENCIES.md`](DEPENDENCIES.md) |
| Pressing **B** closes the game | You are running something built without this port's host layer |
| Uploads fail with "cannot find the path" | The app has never been launched (section 6) |
| `No certificate with thumbprint ...` while it is clearly there | Your `PSModulePath` has been rewritten, usually by a PowerShell 7 install, and the `Cert:` drive is gone in 5.1. `package_uwp.py` reads the store through .NET for exactly this reason |
