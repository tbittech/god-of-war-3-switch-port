# gow3-nx — God of War III (PS3) → Nintendo Switch

Port experimental por **recompilación estática**: el código PPU y SPU del `EBOOT.BIN` de PS3 se traduce por adelantado a C/C++, se compila con devkitA64/libnx y sale un `.nro` para homebrew. No es un emulador ni usa JIT.

> **Este repositorio no contiene el juego ni código derivado de él.** No hay EBOOT, PSARC, videos, audio, ni el código PPU/SPU generado por el lifter. Para usarlo hay que tener un dump propio del disco (BCUS-98111 v1.00).

**Estado:** arranca en consola, reproduce el video de intro y muestra la pantalla de calibración de brillo con los shaders del juego. **Todavía no es jugable.**

---

## Progreso general: ~20 %

```
Material y análisis     ██████████  100 %
Arranque (boot → menú)  ██████░░░░   60 %
Gráficos (RSX → Vulkan) ███▌░░░░░░   35 %
Capa Switch (Horizon)   █████▌░░░░   55 %
Rendimiento (30 fps)    ▌░░░░░░░░░    5 %
Juego completo          ░░░░░░░░░░    0 %
Traducción completa     ░░░░░░░░░░    0 %
```

El porcentaje se calcula por fases y con peso: el rendimiento y el recorrido del juego entero son la mayor parte del trabajo que falta.

## Lo que ya funciona (verificado en consola)

| Área | Estado |
|---|---|
| Lift del PPU (todo el EBOOT) y de las SPU (8 ELF + PM de WWS + 19 jobs relocables) | ✅ |
| HLE de LV2/PRX: hilos, lwmutex/lwcond, event queues, FS sobre `sdmc:` | ✅ |
| Lectura de los PSARC, Edge zlib en SPU | ✅ |
| SPURS: tasksets, jobs WWS, señales (MLAA cierra su pipeline por frame) | ✅ parcial |
| RSX: FIFO real, semáforos, parking de command buffers de EDGE | ✅ |
| Vulkan sobre **NVK** (mesa para Tegra X1): swapchain 1280×720 | ✅ |
| Shaders del juego: RSX → HLSL → SPIR-V (glslang en consola), 32/32 pipelines OK | ✅ |
| Video `cellVdec` (MPEG-2) sin FFmpeg: el stream termina y el juego sigue | ✅ (cuadros en negro) |
| Logos y créditos del estudio en pantalla (~45 fps) | ✅ |
| Pantalla de calibración de brillo bien dibujada | ✅ |

## Dónde está trabado ahora

1. **Hilos SPU asíncronos:** `pthread_create` da ENOMEM con 6 hilos vivos (stacks de 16 MB sacados del heap). Los jobs que quedan en la cola diferida no corren. El fix planeado es un pool fijo de workers y stacks más chicos.
2. **Input:** el pad (A = CROSS) todavía no avanza la pantalla de brillo.
3. **`cellSpursJoinTask2`** sin implementar.

## Lo que falta

### Para llegar a jugar
- [ ] Pool de workers SPU y presupuesto de memoria (heap vs. VRAM vs. espacio guest de 4 GB)
- [ ] Input por libnx/SDL hasta el menú principal
- [ ] Estado gráfico que todavía no se respeta: blending, viewport/scissor, stencil, culling, samplers
- [ ] Video real (FFmpeg estático de `switch-ffmpeg` o decodificador MPEG-2 propio) y audio (XVAG/ATRAC3)
- [ ] Guardado (`cellSaveData`)

### Rendimiento (objetivo: 30 fps sin overclock)
- [ ] Perfilar por subsistema: PPU, cada job SPU, walker del RSX, GPU
- [ ] Sacar del intérprete los jobs WWS que todavía pasan por ahí (registrar las regiones y los 19 reloc)
- [ ] MLAA por SPU → AA por shader en GPU (o apagarlo)
- [ ] Jobs SPU calientes (skinning, culling, partículas) reescritos en C++ con NEON
- [ ] Resolución dinámica (720p en dock, 540-600p en portátil), sombras, LOD y mips más bajos
- [ ] Posibles parches de RPCS3 para BCUS-98111 1.00: MLAA, DoF, motion blur, bloom

### Traducción
- [ ] El disco US (BCUS-98111) trae **solo `ENGLISH.PSARC`**. Para tener textos y voces en español hace falta el `.PSARC` de idioma de una versión europea/latina (dump propio) o una traducción de los textos hecha desde cero.
- [ ] Herramienta para extraer, editar y volver a empaquetar los textos (`LANGS_*.TXT` + PSARC)
- [ ] Fuentes con caracteres en español (á é í ó ú ñ ¿ ¡)

## Mediciones

| Corrida | Qué pasó | fps |
|---|---|---|
| 10ª | RSX null, flips estables 9 min | ~4,4 |
| 12ª | Vulkan/NVK + shaders, créditos en pantalla | ~8 |
| 13ª | Logos / video de intro | ~45 / ~3,8 |
| 16ª | El video termina, pantalla de brillo | 1 flip cada ~20 s (jobs SPU bloqueados) |

## Cómo está armado

- **Base:** [`sp00nznet/ps3recomp`](https://github.com/sp00nznet/ps3recomp) (MIT): lifters, runtime y HLE, con parches para Switch (`__SWITCH__`).
- **Referencias:** `andrebrumdev/gow2-recomp` y `rigaz29/Ascension-R` (mismo engine de SMS), RPCS3.
- **Gráficos:** backend Vulkan de ps3recomp sobre NVK estático (`libvulkan.a`) con `VK_NN_vi_surface`.
- **Pruebas:** nxlink + sys-botbase (capturas y botones por red).

## Compilar

Hace falta devkitPro (devkitA64, libnx, switch-sdl2), el SDK de NVK para Switch y tu propio dump.

| Carpeta | Qué hay |
|---|---|
| `port/` | `main.cpp` (arranque, entorno del título), HLE extra (`lfqueue.cpp`, `stubs.cpp`) y el CMake del juego |
| `nx/` | Capa Switch: memoria guest de 4 GB, excepciones, compat POSIX, shims de glslang |
| `patches/` | Parches a ps3recomp para Switch |
| `tools/` | Lift de los jobs SPU relocables de WWS |
| `docs/` | Roadmap por fases |

Pasos: clonar ps3recomp en `de4f090` y aplicar el parche, correr el pipeline de ps3recomp sobre tu `EBOOT.ELF` desencriptado (salida en `port/ppu/` y `port/spu/`) y compilar `port/` con el toolchain de Switch. La guía detallada va a llegar cuando el port arranque hasta el menú.

## Legal

Proyecto de investigación sin fines de lucro. God of War es propiedad de Sony Interactive Entertainment. No se distribuyen assets, binarios del juego ni código generado a partir de ellos.
