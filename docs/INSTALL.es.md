# Instalación — de una copia limpia al juego corriendo en la consola

*(English: [INSTALL.md](INSTALL.md))*

Ocho apartados. Del 1 al 5 pasan en tu PC; el 6 y el 7, en la consola. Donde un paso tiene trampa,
la trampa está en el paso y no en una nota al pie.

## 0. Dónde se escriben estas órdenes

- **PowerShell** — pulsa la tecla **Windows**, escribe `powershell` y pulsa **Entrar**. Para los
  scripts de Python, el certificado y el Device Portal.
- **Una ventana de `cmd` con el entorno de compilación cargado** — pulsa **Windows**, escribe `cmd`,
  **Entrar**, y ejecuta dentro la línea de `vcvarsall.bat` del apartado 3. Para CMake y Ninja. El
  entorno solo dura lo que dure esa ventana.

**No hace falta «ejecutar como administrador»**, y usarlo rompería el apartado 4: el certificado se
crea en el almacén de quien ejecuta la orden, así que una ventana elevada lo deja donde el paso de
la firma no lo encuentra. El `-ExecutionPolicy Bypass` de los ejemplos es porque un Windows de
fábrica se niega a ejecutar archivos `.ps1`; vale solo para ese proceso y no cambia nada de tu
sistema.

## 1. Clonar el proyecto del juego

```powershell
git clone --branch v0.0.5 https://github.com/elliotttate/DKC3Recomp.git C:\dkc3
cd C:\dkc3
git submodule update --init --depth 1 snesrecomp
```

**La ruta, corta** (`C:\dkc3`, no una carpeta en lo hondo de Documentos). Con una larga, la
compilación muere con `C1083: Cannot open compiler generated file`, que es `MAX_PATH` y parece un
error del código.

## 2. Parchear y generar la recompilación

El parche primero, que son dos líneas y es rápido:

```powershell
python tools\apply_uwp.py C:\dkc3
```

Comprueba antes de escribir, es idempotente y `--list` explica qué hace: las dos llamadas a
`MessageBoxA` del motor pasan a `OutputDebugStringA`, porque `user32` no existe dentro de un
AppContainer y basta con importarlo para que el paquete se instale y luego no active.

Luego la recompilación, **desde tu propia ROM**, con el script del propio proyecto:

```powershell
python scripts\generate_snesrecomp.py --rom "<tu rom>.sfc" --analysis-backend native
```

Escribe del orden de 3 926 archivos `.c` en `generated/snesrecomp/`. Eso es el juego
traducido a C: por eso este repositorio no los lleva ni puede llevarlos.

## 3. Compilar

En una ventana de **`cmd`**:

```
"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 store
```

```
cmake -S "<este repo>\src" -B C:\dkc3b -G Ninja ^
  -DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION=10.0 ^
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL ^
  -DDKC3_SRC=C:/dkc3

cmake --build C:\dkc3b --target dkc3_uwp
```

- **`x64 store`, no `vcvars64`.** Es lo que enlaza el CRT «_app», que es el que tiene la consola.
  Con el de escritorio el paquete se instala y la consola dice *«the app didn't start»*
  (`0x8027025B`).
- **`-DDKC3_SRC` va con barras normales**, incluso en Windows: con backslashes CMake las lee como
  escapes.
- Es una compilación grande —aquí fueron **3,978 objetos**— porque cada banco del juego es una
  unidad de traducción.
- Lo que sale es `C:\dkc3b\DKC3Recomp.exe`.

## 4. Un certificado para firmar

Una orden, en **PowerShell**, en una ventana **normal**:

```powershell
New-SelfSignedCertificate -Type Custom -Subject "CN=<tu nombre>" -KeyUsage DigitalSignature `
  -FriendlyName "<tu nombre> UWP dev" -CertStoreLocation "Cert:\CurrentUser\My" `
  -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3", "2.5.29.19={text}")
```

Apunta el `Thumbprint` y abre [`../package/Package.appxmanifest`](../package/Package.appxmanifest)
para cambiar los dos `CHANGE-ME`. El `Publisher` tiene que ser **exactamente** el `-Subject` de
arriba, con el `CN=`: la consola compara las dos cadenas y rechaza el paquete si no coinciden. El
`Name` y el `Publisher` forman la familia por la que la consola identifica el juego, y de ella
cuelga el `LocalState` con tus partidas: elígelo una vez y déjalo.

## 5. Empaquetar, firmar e instalar

Primero tus mosaicos ([`ASSETS.es.md`](ASSETS.es.md) lista los trece y sus medidas; este
repositorio no trae ninguno, porque los de la consola se hicieron con el arte del juego).

```powershell
python tools\package_uwp.py --exe C:\dkc3b\DKC3Recomp.exe ^
  --rom "<tu rom>.sfc" --tiles <tu carpeta de mosaicos> ^
  --certificate <tu huella> --version 0.1.0.0 --work C:\dkc3pkg
```

Antes de empaquetar comprueba las cinco cosas que separan un paquete que se instala de uno que
arranca: el bit `APPCONTAINER`, el CRT «_app», todos los DLL importados, el `Publisher` contra el
certificado y los trece mosaicos. Aquí eso dio un `.msix` firmado de unos 9,385,255 bytes.

Instálalo desde el **Device Portal** de la consola (`https://<ip de la consola>:11443`, usuario
`xbox`): **Add** → el `.msix` → y, como dependencia, el appx de `Microsoft.VCLibs.140.00` para x64
del SDK de Windows. La consola necesita **un usuario conectado**; sin nadie conectado, el paquete se
instala y al abrirlo no pasa nada.

⚠ Justo después de instalar, lanzarlo puede fallar con *«Failed to launch the application»*. El
listado de paquetes del portal se queda un momento con la versión vieja y la nueva a la vez, y un
modal del sistema en pantalla hace lo mismo. Espera, quita lo que haya en la tele y reintenta.

## 6. Subir archivos a la app

Dos cosas que cuestan una tarde cada una:

- La carpeta de datos de la app **no existe hasta su primer arranque**. Lanza el juego una vez,
  ciérralo y solo entonces sube algo; si no, la subida falla con «cannot find the path».
- **Sube los archivos de uno en uno, no en un `.zip`.** Cuando el Device Portal extrae un archivo
  comprimido, deja los archivos con permisos que el AppContainer no puede abrir, y la app se
  comporta como si no estuvieran.

## 7. Jugarlo

Conecta el mando **antes** de lanzarlo. La **B** funciona como esperas dentro del juego: el host la
marca como atendida, que es lo que evita que el shell de la consola cierre la app.

## 8. Cuando algo sale mal

| Lo que ves | Qué es |
|---|---|
| `C1083: Cannot open compiler generated file` | La copia o la ruta del build son demasiado largas (`MAX_PATH`) |
| *«the app didn't start»* / `0x8027025B` | El CRT de escritorio (compilaste con `vcvars64`), o falta un DLL en el paquete. `package_uwp.py` caza los dos antes de firmar |
| La consola rechaza el paquete al instalar | El `Publisher` y el sujeto del certificado no son la misma cadena (apartado 4) |
| La consola lo rechaza al activarlo | Falta el bit `APPCONTAINER`: una compilación que no salió de `src/CMakeLists.txt` |
| Rechaza tu ROM | No es el volcado USA exacto sin cabecera. El SHA-256 que espera está en [`DEPENDENCIES.es.md`](DEPENDENCIES.es.md) |
| Pulsar **B** cierra el juego | Estás ejecutando algo compilado sin la capa de host de este port |
| Las subidas fallan con «cannot find the path» | La app no se ha lanzado nunca (apartado 6) |
| `No certificate with thumbprint ...` cuando está ahí claramente | Tu `PSModulePath` está reescrito, normalmente por una instalación de PowerShell 7, y en la 5.1 ya no existe el disco `Cert:`. `package_uwp.py` lee el almacén por .NET justo por esto |
