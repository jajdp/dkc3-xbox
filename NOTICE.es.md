# Aviso

*(English: [NOTICE.md](NOTICE.md))*

## Sin afiliación

Este es un proyecto de aficionado, no oficial y sin ánimo de lucro. **No está afiliado,
patrocinado ni relacionado con Nintendo ni con Microsoft**, ni con los autores de
[elliotttate/DKC3Recomp](https://github.com/elliotttate/DKC3Recomp) o [snesrecomp](https://github.com/mstan/snesrecomp). *Donkey Kong Country 3: Dixie Kong's Double Trouble!*,
*Xbox* y todos los nombres y marcas relacionados son de sus respectivos propietarios.

Aquí no se vende nada y nada está monetizado.

## Qué NO se distribuye en este repositorio

Ninguna ROM. Ningún código del juego de Nintendo. Ningún fuente recompilado —esos se producen en
tu máquina, desde tu volcado, con el script del propio proyecto—. Ningún ejecutable. Ningún
paquete `.msix`. Ninguna partida. Ningún mosaico, que se hicieron con el arte del propio juego.
Ningún certificado de firma, ni la huella de uno.

El `.gitignore` bloquea todo eso por extensión. Es una red de seguridad, no un sustituto de leer
este archivo.

## Qué contiene de trabajo ajeno

Medido, para que «no distribuimos nada suyo» sea algo que puedas comprobar y no algo que tengas
que creerte:

| | Líneas | Qué es |
|---|---|---|
| Código propio | 1 299 | `src/uwp_host.cpp`, `src/uwp_shaders.h`, `src/uwp_shaders.hlsl` y `src/CMakeLists.txt`: la capa de host de UWP, el presentador de Direct3D 11 y el objetivo de compilación. Escrito aquí |
| Herramientas propias | 459 | `tools/apply_uwp.py` y `tools/package_uwp.py` |
| Líneas propias dentro del parche | 2 | Dos llamadas a `OutputDebugStringA` en lugar de dos a `MessageBoxA` |
| Citado de snesrecomp | 12 | Las **líneas de contexto** que un diff unificado tiene que llevar para saber dónde va el cambio, más las 2 que quita. Nada más suyo está aquí |

El motor del que salen esas 14 líneas está licenciado bajo la
[PolyForm Noncommercial License 1.0.0](https://polyformproject.org/licenses/noncommercial/1.0.0),
que permite cambios y obras derivadas con fines no comerciales, y este repositorio se publica bajo
esa misma licencia para que encaje. El proyecto del juego es MIT, y aquí no se reproduce ni una
línea suya: la capa de host que este port sustituye es nuestra.

La ROM la pones tú y nunca pasa por aquí. Lo que la receta lleva sobre ella es un **hash SHA-256**:
una huella, no contenido.

## Retirada

Si tienes derechos sobre algo de lo anterior y quieres que se retire, escribe a
**jajdpmail@gmail.com** y se retira. Sin discusión y sin demora.
