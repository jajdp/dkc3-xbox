# Dependencias

*(English: [DEPENDENCIES.md](DEPENDENCIES.md))*

Todo lo de aquí está medido en la máquina donde se hizo el port. Donde la versión importa, se dice
por qué.

## El proyecto del juego

| | |
|---|---|
| Repositorio | [elliotttate/DKC3Recomp](https://github.com/elliotttate/DKC3Recomp) |
| Etiqueta | **`v0.0.5`** — el parche se genera contra el submódulo del motor que fija esa etiqueta |
| Submódulo `snesrecomp` | Obligatorio. Es el motor, y el parche se aplica **dentro** de él: `git submodule update --init --depth 1 snesrecomp` |
| Licencia | MIT el proyecto del juego, PolyForm Noncommercial el motor |

**La ruta, corta.** Una copia en algo como `C:\dkc3` te ahorra una compilación que muere con
`C1083: Cannot open compiler generated file`, que es `MAX_PATH` y no un problema del código.

## Tu propia ROM

| | |
|---|---|
| Archivo | Un volcado `.sfc` de *Donkey Kong Country 3: Dixie Kong's Double Trouble!* (USA, **sin cabecera**) |
| SHA-256 | `2277a2d8dddb01fe5cb0ae9a0fa225d42b3a11adccaeafa18e3c339b3794a32b` |
| Tamaño | 4 194 304 bytes |

El proyecto comprueba el volcado él mismo y rechaza cualquier otro, así que con otra revisión no
vas a obtener una compilación. Aquí no se distribuye nada de la ROM; el hash es una huella.

La recompilación se genera **desde tu volcado**, en tu máquina, con el script del propio proyecto:
del orden de 3 926 archivos `.c` en `generated/snesrecomp/`. Por eso esos archivos están en
el `.gitignore`: son código del juego.

## Herramientas de compilación

| | Versión usada | ¿Importa la versión? |
|---|---|---|
| Visual Studio 2022 | Community, MSVC 14.44 | **No**, pero el *entorno* sí: `vcvarsall.bat x64 store`, nunca `vcvars64.bat`. Es lo que enlaza contra el CRT «_app», que es el que tiene la consola. La carga de trabajo de C++ para UWP **no hace falta**: empaquetan las herramientas del SDK, no MSBuild |
| SDK de Windows | 10.0.26100.0 | **No.** `package_uwp.py` busca el SDK más nuevo que tenga `makeappx`, `makepri` y `signtool` |
| CMake | 3.20 o posterior | Viene con Visual Studio 2022 |
| Ninja | Cualquiera | Viene con Visual Studio 2022. ⚠ Su generador de WindowsStore **no** pasa `/APPCONTAINER`; `src/CMakeLists.txt` lo pasa a mano |
| Python 3 | 3.13 | **No.** Solo biblioteca estándar. Con 3.8 o posterior vale |
| PowerShell | La 5.1 que ya trae Windows | **PowerShell 7 no es una dependencia.** Los ejemplos llevan `-ExecutionPolicy Bypass` porque un Windows de fábrica está en `Restricted` |

**Sin SDL y sin OpenGL.** Este port sustituye la capa de host en vez de portarla, así que no hay
biblioteca de gráficos o de entrada de la que haya que encontrar una versión para UWP. Los dos
shaders van ya compilados a bytecode en `src/uwp_shaders.h`, así que tampoco hace falta un
compilador de shaders.

## La consola

| | |
|---|---|
| Hardware | Xbox Series X o Series S |
| Modo | **Modo desarrollador**, con Dev Home instalado |
| Acceso | El Device Portal por la red, y **un usuario conectado** en la consola — sin nadie conectado, el paquete se instala y al abrirlo no pasa nada |
| Dependencia en la consola | `Microsoft.VCLibs.140.00`. Se declara en el manifiesto y se instala junto al paquete, no va dentro |
