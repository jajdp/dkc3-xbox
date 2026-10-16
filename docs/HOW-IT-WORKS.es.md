# Cómo funciona

*(English: [HOW-IT-WORKS.md](HOW-IT-WORKS.md))*

## La decisión que define este port

Un proceso UWP en una Xbox no puede usar OpenGL, y el renderizador de Direct3D de SDL pide un
`HWND` que en UWP no existe. El host de escritorio del juego está construido sobre OpenGL.

Así que el host **no se porta: se sustituye**. El motor ya produce el cuadro terminado en memoria;
`src/uwp_host.cpp` es una capa de host que abre la `CoreWindow`, sube ese cuadro como textura de
Direct3D 11, lo dibuja con dos triángulos, lee el mando por `Windows.Gaming.Input` y reproduce el
audio que el motor le entrega. 1 299 líneas, y ni una del código del juego.

Por eso también `patches/` está casi vacío: cuando sustituyes el host en vez de parchearlo, lo
único que queda por cambiar en el proyecto del juego son las dos llamadas que arrastran `user32`.

## Las tres cosas que la consola rechaza

1. **`user32`.** Ahí vive `MessageBoxA`, el motor la llama dos veces en el error de ROM equivocada,
   y dentro de un AppContainer esa biblioteca no existe. Un paquete que solo la *importa* se
   instala y luego no activa. El parche convierte las dos llamadas en `OutputDebugStringA`, que
   sigue llegando a un depurador y al registro de la consola.
2. **El CRT de escritorio.** Compilado en el entorno normal `vcvars64`, el ejecutable importa
   `MSVCP140.dll`; la consola tiene la variante «_app». Mismo síntoma: se instala, no arranca,
   `0x8027025B`. `vcvarsall.bat x64 store` es todo el arreglo, y `package_uwp.py` se niega a
   empaquetar un ejecutable sin el CRT «_app».
3. **El bit `APPCONTAINER`.** El generador de WindowsStore de Ninja no lo pone, y sin él el loader
   rechaza el paquete al activarlo. `src/CMakeLists.txt` pasa `/APPCONTAINER` a mano.

## La B no es un botón

En una Xbox, la **B** es el «atrás» del sistema. Si la app no marca el evento de tecla como
atendido, el shell se la lleva por delante: el juego parece caerse al pulsar B. El host atiende
`GamepadB`, `GoBack` y `Escape` en `CoreWindow::KeyDown` y los marca como atendidos. Son dos
líneas, y es obligatorio en cualquier host UWP para consola.

## Empaquetar el ejecutable equivocado

El único fallo de aquí que no es técnico. `package_uwp.py` acepta cualquier `--exe` que pase sus
comprobaciones, y esas comprobaciones no pueden distinguir una compilación al día de una vieja que
estaba en otro árbol de build. Así es como el paquete instalado en las consolas (0.0.4.0) acabó
**sin el código de guardado que el fuente ya tenía**: se armó con un ejecutable anterior.

Medido el 2026-10-14: el ejecutable que construye esta receta es 10 752 bytes mas, 7 168 de ellos en `.text`: el host ha avanzado desde que se armo ese paquete. Las cadenas
`%s/save.srm`, `Failed fopen for save: %s` y todo el manejador de `Suspending` están en él y no en
el paquete instalado.

La lección es barata de aplicar: empaqueta directamente del build que acabas de correr y, si en la
consola falta algo que sabes que está en el fuente, sospecha del ejecutable antes que del código.

## Por qué aquí MAX_PATH importa más de lo normal

Cada banco del juego es una unidad de traducción aparte, así que esto es una compilación de
3,978 objetos. Las rutas de los objetos reflejan las de los fuentes, y basta con una copia unas
carpetas adentro para que el compilador falle con `C1083: Cannot open compiler generated file`, que
suena a cadena de compilación rota y es solo una ruta larga.

## Cómo se verificó esto

No leyéndolo: reconstruyéndolo. El 2026-10-14, desde el `src/` y el `patches/` de este
repositorio, contra una copia con la recompilación ya generada:

| Qué se midió | Resultado |
|---|---|
| El parche aplica, revierte y es idempotente | Sí; a la segunda corrida dice `already applied` y no escribe nada |
| ¿Compila? | **3,978 objetos, 0 errores**, y enlaza `DKC3Recomp.exe` (9,561,088 bytes) |
| Paquete y firma | `makepri` → `makeappx` → `signtool`, un `.msix` firmado de 9,385,255 bytes |
| Contra el `.msix` que corre en las consolas (0.0.4.0) | 10 752 bytes mas, 7 168 de ellos en `.text`: el host ha avanzado desde que se armo ese paquete |
| Determinismo de la cadena de compilación | Medido una vez para esta familia en vez de tres: el mismo árbol compilado desde dos rutas distintas da **93 de 93 objetos con `.text` idéntico** (ver el repositorio `supermetroid-recomp-xbox`, mismo motor, mismo compilador, mismas opciones) |

## Sobre el idioma

Los comentarios del host y sus cadenas de traza están en español, que es donde se escribieron. Se
publican tal como se compilaron y se verificaron, en vez de volver a teclearlos para publicarlos;
lo que imprimen los scripts, y toda esta documentación, está en inglés y en español.
