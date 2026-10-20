# Donkey Kong Country 3: Dixie Kong's Double Trouble! en la Xbox — el port UWP

Cómo coger la recompilación estática [DKC3Recomp](https://github.com/elliotttate/DKC3Recomp) de *Donkey Kong Country 3: Dixie Kong's Double Trouble!* (Super Nintendo,
1996) y hacerla correr **en una Xbox Series X en modo desarrollador**, como un paquete UWP que
compilas y firmas tú.

*(English: [README.md](README.md))*

No es una teoría: el juego arranca, se juega con mando a **59,9 cuadros por segundo**, con sonido y
en 16:9, y el paquete que corre en dos consolas se construyó exactamente con esta receta.

## En qué consola se probó

**En una Xbox Series X**, en modo desarrollador. Las dos consolas en las que se construyó y se probó esto
son Series X, así que es el único hardware al que se refiere cualquier cosa que se afirme aquí.

**En una Series S no se ha probado nunca, ni una vez.** Es otra máquina: menos memoria disponible para un
juego y una GPU más floja. Puede que funcione en una, puede que vaya más despacio, o puede que no
arranque; nada de aquí se ha medido en ella. Si lo ejecutas en una Series S, lo que obtengas no es lo que
dice este README, y ninguna de las cifras de aquí es una promesa sobre esa consola.

## Qué es este repositorio

**Una receta y las herramientas que la aplican.** Nadie había publicado una versión UWP de este
juego. El port **sustituye** la capa de host del juego —la que abre ventana, dibuja y lee el
mando— por una escrita para UWP que presenta con **Direct3D 11** sobre la `CoreWindow` de la
consola. No porta OpenGL y no usa SDL: el motor ya produce el cuadro terminado, y esto lo dibuja.

Lo que se aplica a tu copia es un parche de dos líneas; todo lo demás son archivos nuevos que se
ponen al lado.

## Qué **no** es

Aquí **no hay ROM, ni código del juego, ni fuentes recompilados, ni ejecutable, ni paquete `.msix`,
ni arte**. Esto no puede dar un juego jugable por sí solo, y nada de lo que te puedas descargar
aquí es un juego. Necesitas tu propio volcado legal del cartucho. Ver [`NOTICE.es.md`](NOTICE.es.md).

Los mosaicos del paquete tampoco están: los de la consola de verdad se hicieron con el arte del
propio juego, que es de Nintendo. Los pones tú — [`docs/ASSETS.es.md`](docs/ASSETS.es.md) lista los
trece archivos y sus medidas, y `package_uwp.py` se detiene con un mensaje claro si falta alguno.



## Qué hace falta antes

| | |
|---|---|
| El proyecto del juego | [elliotttate/DKC3Recomp](https://github.com/elliotttate/DKC3Recomp) `v0.0.5`, clonado con su submódulo `snesrecomp`, **compilando y corriendo antes en tu PC** |
| Tu propia ROM | Un volcado `.sfc` de *Donkey Kong Country 3: Dixie Kong's Double Trouble!* (USA, sin cabecera). El SHA-256 `2277a2d8dddb01fe5cb0ae9a0fa225d42b3a11adccaeafa18e3c339b3794a32b` es el que se usó aquí, y la comprobación de ROM del propio proyecto rechaza cualquier otra |
| Visual Studio | 2022, con las herramientas de C++. Lo que importa es **en qué entorno compilas**: `vcvarsall.bat x64 store`, nunca `vcvars64.bat` ([`docs/DEPENDENCIES.es.md`](docs/DEPENDENCIES.es.md)) |
| SDK de Windows | Cualquiera reciente de 10/11; `package_uwp.py` busca el más nuevo él solo |
| CMake y Ninja | Los dos vienen con Visual Studio 2022 |
| Python 3 | Para el parche y el empaquetado. Solo biblioteca estándar |
| Una Xbox en modo desarrollador | Y un certificado para firmar — uno autofirmado vale ([`docs/INSTALL.es.md`](docs/INSTALL.es.md) apartado 4) |

Sin SDL, sin OpenGL y sin compilador de shaders en ejecución: los dos shaders van compilados a
bytecode en `src/uwp_shaders.h`.

## Cómo va

Cuatro pasos, y la versión larga de cada uno está en [`docs/INSTALL.es.md`](docs/INSTALL.es.md).

> **Dónde se escribe esto:** en **PowerShell** — pulsa la tecla **Windows**, escribe `powershell` y
> pulsa **Entrar** — en una ventana **normal**: no hace falta «ejecutar como administrador», y
> usarlo rompería el paso de la firma, porque el certificado vive en el almacén de quien ejecuta la
> orden. Desde la carpeta de este repositorio.

```powershell
# 1. parchear tu copia (dos líneas, en el submódulo del motor; idempotente y comprobado antes)
python tools\apply_uwp.py <ruta-a-tu-copia>

# 2. generar la recompilación desde TU rom (este script es del propio proyecto)
#    python scripts\generate_snesrecomp.py --rom "<tu rom>.sfc" --analysis-backend native
#    -> del orden de 3 926 archivos .c en generated/snesrecomp/

# 3. configurar y compilar, en una ventana donde hayas cargado `vcvarsall.bat x64 store`
cmake -S src -B <ruta-de-build-corta> -G Ninja -DCMAKE_SYSTEM_NAME=WindowsStore ^
      -DCMAKE_SYSTEM_VERSION=10.0 -DCMAKE_BUILD_TYPE=Release ^
      -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL -DDKC3_SRC=<ruta-a-tu-copia>
cmake --build <ruta-de-build-corta> --target dkc3_uwp

# 4. empaquetar y firmar (tus mosaicos, tu certificado)
python tools\package_uwp.py --exe <ruta-de-build-corta>\DKC3Recomp.exe ^
       --rom "<tu rom>.sfc" --tiles <tu carpeta de mosaicos> ^
       --certificate <tu huella> --work <carpeta de trabajo>
```

Después se instala el `.msix` por el Device Portal. Paso a paso, incluido el lado de la consola:
**[`docs/INSTALL.es.md`](docs/INSTALL.es.md)**.

## Qué hubo que resolver

Cada una de estas cosas fue un fallo real en la consola. El detalle está en
**[`docs/HOW-IT-WORKS.es.md`](docs/HOW-IT-WORKS.es.md)**.

| | Síntoma | Arreglo |
|---|---|---|
| 1 | En una Xbox no hay OpenGL, y el renderizador de SDL pide un `HWND` que UWP no tiene | La capa de host se sustituye, no se porta: `src/uwp_host.cpp` presenta el cuadro terminado con Direct3D 11 sobre la `CoreWindow` |
| 2 | El paquete se instalaba y luego no arrancaba (`0x8027025B`) | Importaba el CRT **de escritorio**. Compilado en el entorno `x64 store` importa el «_app», que es el que tiene la consola |
| 3 | La consola rechazaba el paquete al activarlo | Faltaba el bit `APPCONTAINER`. El generador de WindowsStore de Ninja no lo pone; `src/CMakeLists.txt` pasa `/APPCONTAINER` a mano |
| 4 | El motor arrastraba `user32` | Dos llamadas a `MessageBoxA` en el error de ROM equivocada. `user32` no existe dentro de un AppContainer, y basta con importarlo para romper la activación. Pasan a `OutputDebugStringA`: ese es el único parche de `patches/` |
| 5 | `C1083: Cannot open compiler generated file` | La ruta del build pasaba de 260 caracteres (`MAX_PATH`). Compila en una ruta corta |
| 6 | Pulsar **B** cerraba el juego | En la Xbox, B es también el «atrás» del sistema, y el shell se lleva la app si no dice que ha atendido la tecla |
| 7 | Los archivos subidos en `.zip` la app no los leía | Al extraerlos, el Device Portal les deja permisos que el AppContainer no abre. Súbelos de uno en uno |
| 8 | Una subida fallaba con «cannot find the path» | La carpeta de datos de la app no existe hasta su **primer arranque**. Lánzalo una vez, ciérralo y luego sube |

## Cómo se verificó

No leyéndolo: reconstruyéndolo. El 2026-10-14, desde el `src/` y el `patches/` de este
repositorio, contra una copia con la recompilación ya generada:

| Qué se midió | Resultado |
|---|---|
| El parche aplica, revierte y es idempotente | Sí; a la segunda corrida dice `already applied` y no escribe nada |
| ¿Compila? | **3,978 objetos, 0 errores**, y enlaza `DKC3Recomp.exe` (9,561,088 bytes) |
| Paquete y firma | `makepri` → `makeappx` → `signtool`, un `.msix` firmado de 9,385,255 bytes |
| Contra el `.msix` que corre en las consolas (0.0.4.0) | 10 752 bytes mas, 7 168 de ellos en `.text`: el host ha avanzado desde que se armo ese paquete |
| Determinismo de la cadena de compilación | Medido una vez para esta familia en vez de tres: el mismo árbol compilado desde dos rutas distintas da **93 de 93 objetos con `.text` idéntico** (ver el repositorio `supermetroid-recomp-xbox`, mismo motor, mismo compilador, mismas opciones) |

## Créditos y licencia

Port y herramientas de **Recompilaciones**. Publicado bajo la
[PolyForm Noncommercial License 1.0.0](LICENSE): el proyecto del juego es MIT, pero este port se
construye sobre el motor **snesrecomp**, que es PolyForm Noncommercial, y el parche de `patches/`
se aplica a ese motor. Quedarse con la más restrictiva de las dos es la lectura honesta.

*Donkey Kong Country 3: Dixie Kong's Double Trouble!* es © Nintendo. Este proyecto no está afiliado con ellos, ni con Microsoft, ni con los
autores del proyecto del juego o del motor, y no distribuye nada que les pertenezca. El detalle
está en [`NOTICE.es.md`](NOTICE.es.md). Los titulares de derechos pueden escribir a
**jajdpmail@gmail.com**.
