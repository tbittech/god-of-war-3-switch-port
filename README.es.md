<h1 align=center>God of War III — port nativo para Nintendo Switch</h1>

[English](README.md) | **Español**

Port nativo para Nintendo Switch de la versión de PS3 de **God of War III** (BCUS-98111).

No es un emulador. El código PPU (PowerPC) y SPU del `EBOOT.BIN` del juego se recompila estáticamente a C/C++ con
[ps3recomp](https://github.com/sp00nznet/ps3recomp), se compila para la Switch con devkitPro/libnx, y el juego se
dibuja con un backend Vulkan que corre sobre NVK (Mesa). Los shaders del RSX se traducen a SPIR-V en la consola.

> [!NOTE]
> Este repositorio **no trae datos del juego ni código derivado del juego**. Necesitas tu propia copia del disco de
> PS3 y el `EBOOT.BIN` desencriptado.

## Estado

> [!IMPORTANT]
> **Pausa hasta la próxima semana:** se me acabaron los tokens de la IA con la que desarrollo el port; se renuevan la
> semana que viene y ahí sigo. El plan está en [PORT_STATUS.md](PORT_STATUS.md).

**Último avance (13/11):** de ~1,9 a **~3 FPS (con OC)** con MLAA off, caché de la LUT de color grading y backoff del
PPU. El cuello ya no son las SPU: es el hilo del RSX, que hace submit + espera de fence por cada draw. Se está
probando el draw batching. Probabilidad estimada de 30 FPS sin OC: ~10 %; de 15-20 FPS: ~30-40 %.

**En desarrollo, todavía no es jugable. Progreso general: ~25 %.**

```text
Material y análisis      ██████████  100 %
Arranque (boot → menú)   ██████░░░░   60 %
Gráficos (RSX → Vulkan)  ███▌░░░░░░   35 %
Capa Switch (Horizon)    █████▌░░░░   55 %
Rendimiento (30 FPS)     █░░░░░░░░░   10 %
Juego completo           ░░░░░░░░░░    0 %
Traducción completa      ░░░░░░░░░░    0 %
```

En la consola, con los relojes de fábrica y sin overclock: arranca, lee los PSARC, corre SPURS y los jobs SPU,
muestra los logos y créditos del estudio (~45 FPS), reproduce el video de intro y dibuja la **pantalla de
calibración de brillo** con los shaders del juego (32 de 32 pipelines bien).

**Dónde está trabado:** pasada la intro corre a ~3 FPS (con OC). El cuello es el hilo del RSX (backend Vulkan), que
espera a la GPU por cada draw; con el draw batching se colgó en el flip 870 y el arreglo está sin probar.

**Objetivos:**
- **30 FPS sin overclock.** Es la meta más difícil: el juego usa mucho las 6 SPU y en la Switch eso corre en 3
  núcleos A57. El plan: pasar el MLAA de SPU a un AA por shader, reescribir con NEON los jobs más pesados,
  resolución dinámica y menos efectos de post (DoF, motion blur).
- **Traducción completa al español.** El disco US trae solo `ENGLISH.PSARC`. Hace falta el PSARC de idioma de una
  versión europea o latina (de tu propio dump) o traducir los textos, más fuentes con á, ñ, ¿ y ¡.

El registro detallado está en [PORT_STATUS.md](PORT_STATUS.md) (lo más nuevo arriba) y el plan por fases en
[docs/ROADMAP.md](docs/ROADMAP.md).

## Instalar en la Switch

Todavía no hay versión para jugar. Cuando llegue al menú se va a publicar el NRO con las instrucciones. Las pruebas
de hoy se hacen así:

```text
/switch/gow3-nx/
  gow3-nx.nro
  PS3_GAME/PARAM.SFO
  USRDIR/            (los PSARC, CINEMATICS/ y SOUNDS/ de tu disco)
```

Hay que abrirlo en modo aplicación completa (manteniendo **R** al abrir un juego instalado, o con un forwarder).
El modo applet (Álbum) no da memoria suficiente.

## Estructura

| Ruta | Qué es |
|---|---|
| `port/` | Código propio del port: `main.cpp` (arranque y entorno del título), HLE extra (`lfqueue.cpp`, `stubs.cpp`) y el CMake del juego |
| `nx/` | Capa Horizon: memoria guest de 4 GB, puente de excepciones, compat POSIX, shims de glslang |
| `patches/` | Cambios para Switch sobre ps3recomp `de4f090` (Vulkan/NVK, cellVdec sin FFmpeg, cola de jobs SPU, FS) |
| `tools/lift_reloc_jobmods.py` | Liftea los 19 jobs SPU relocables de WWS |
| `docs/ROADMAP.md` | Plan por fases |

## Compilar (resumen)

1. Instala devkitPro (devkitA64 + libnx + switch-sdl2) y consigue el SDK de NVK para Switch.
2. Clona ps3recomp en `de4f090` y aplica `patches/ps3recomp-switch.patch`.
3. Desencripta tu `EBOOT.BIN` (RPCS3: Utilities → Decrypt PS3 Binaries) y corre el pipeline de ps3recomp
   (la salida va a `port/ppu/` y `port/spu/`, que nunca se suben al repo).
4. Configura y compila `port/` con el toolchain de Switch.

## Créditos

- [ps3recomp](https://github.com/sp00nznet/ps3recomp) (MIT): lifters, runtime y HLE.
- [gow2-recomp](https://github.com/andrebrumdev/gow2-recomp) y [Ascension-R](https://github.com/rigaz29/Ascension-R),
  referencias del mismo motor de Santa Monica Studio.
- [RPCS3](https://github.com/RPCS3/rpcs3), devkitPro, libnx, glslang y Mesa/NVK.

God of War es propiedad de Sony Interactive Entertainment. Proyecto sin fines de lucro.

## Apoyo

Si te gusta mi trabajo y quieres apoyarme:

<a href="https://tecito.app/tebit"><img src="https://img.shields.io/badge/Invitame_un-tecito-6d4aff?style=for-the-badge" alt="Invitame un tecito" height="32"></a>
