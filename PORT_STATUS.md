# gow3-nx — registro del port

Lo más nuevo arriba. Todas las pruebas son en consola por nxlink. Las mediciones marcadas **OC** son con overclock; la meta (30 FPS) se mide sin OC.

## ⏸️ Pausa hasta la próxima semana
El desarrollo se frena unos días: **se me acabaron los tokens de la IA con la que trabajo el port y se renuevan la
semana que viene.** El estado de abajo es exactamente donde quedó; se retoma desde el punto 1 del plan.

### Plan para la próxima semana (en orden)
1. **Probar el build pendiente (47ª):** draw batching de Vulkan (`VK_DRAW_BATCH=1`) + flush del lote al final de cada
   pasada del FIFO. Con batching solo, el juego se colgó en el flip 870.
   - Si ya no se cuelga: medir ms/frame en la fase de juego contra la 45ª (~355 ms/frame) y perfilar de nuevo el hilo del RSX.
   - Si se cuelga igual: revisar la carrera del estado del flip (`cellGcmResetFlipStatus` contra la entrega del tick)
     y la señal de `GET_REPORT` a las tareas SPU; buscar en Ghidra quién escribe el u16 que el PPU espera en `0x26D3D0`.
2. **Bajar el costo del hilo del RSX** (hoy el techo, ~200 ms/frame): menos esperas de fence (la de NVK duerme en
   sondeo), readbacks de color/zdepth sin bloquear, present encolado (como RPCS3) en vez de esperar dentro del hilo.
3. **Solapar PPU y RSX:** hoy corren casi en serie (el PPU espera ~150 de cada 339 ms).
4. Con el RSX liviano, volver al **mantra**: HLE de jobs SPU (Big 111/112, cobertura del lift como en Ascension-R),
   parches de post de RPCS3 (DoF, motion blur, bloom, fog) como interruptores, resolución dinámica + FSR1.

### Probabilidades (estimación honesta, hoy)
| Meta | Probabilidad |
|---|---|
| Que corra estable hasta el gameplay | ~70 % |
| 10 FPS sin OC | ~45 % |
| 15-20 FPS | ~30-40 % |
| 30 FPS sin OC | ~10 % |

## 2026-11-13 — de ~1,9 a ~3 FPS (OC); el cuello pasó de las SPU al hilo del RSX
Progreso medido en consola (flips por segundo, OC):

| Prueba | Cambio | FPS (OC) |
|---|---|---|
| 31ª-34ª | CPU por núcleo y por hilo: los 3 núcleos disponibles al 97-99,8 % | ~1,9 |
| 35ª | Backoff (sleep) en la espera de job-counter del PPU: PPU 84 → 58 % de CPU | ~2,15 (+12 %) |
| 39ª | `SPU_LS_NO_DIAG` (portado de Ascension-R) | ~2,20 (+3 %) |
| 40ª | **MLAA off** (parche de RPCS3 1.00 como interruptor `GOW3_NO_MLAA`) | **~2,98 (+35 %)** |
| 43ª | Caché por hash de la LUT 3D de color grading dentro del job SPU (bit-idéntico, `bad 0`) | ~3 (Little libera CPU) |

**Hallazgos:**
- **Perfilador SPU por job module** (`sprof`): el worker "Little" era casi todo `rjm_42d900` = la **LUT 3D de color
  grading** (32×32×32 RGBA8, 61 % de un hilo, la misma cada frame). Cacheada: ya no cuesta.
- El float SPU ya iba por NEON con FPCR RZ+FZ; el camino escalar frío es <1 % (descartado como costo).
- **Trazas (`PS3_FTRACE`, 44ª):** el PPU no está limitado por las SPU sino que **espera al RSX**: frame de ~339 ms,
  drain del RSX ~200 ms/frame con una pasada de hasta 154 ms. PPU y RSX quedan casi en serie.
- **Muestreador del hilo del RSX en Horizon** (`RSX_WALKER_PROF`, con `svcGetThreadContext3`; 45ª): el hilo pasa
  ~90 % esperando: `vk_submit_and_wait` **por cada draw** (submit 33 %, espera de fence 34 % + 23 %). Trabajo real <10 %.
- `env.txt` en la SD (`sdmc:/switch/gow3-nx/env.txt`) pisa los defaults: los A/B ya no necesitan recompilar.
- Otros parches 1.00 listos para usar: DoF, motion blur, layered fog y bloom.

**En curso:** draw batching (hasta 512 draws por submit). La 46ª se colgó en el flip 870 (hilo del RSX ocioso, FIFO
vacío, PPU esperando un u16 en `0x26D3D0`); el build con flush al final de cada pasada está compilado, sin probar.

**Descartado:** `GOW3_NO_COLORGRADE` como parche (cuelga a los 51 s), dar 2 workers a Little, fijar hilos a núcleos
(no hay núcleo libre), reescribir el float SPU a NEON (ya lo era).

Herramientas nuevas en `tools/`: `wbase.py` (base de carga del NRO desde las muestras) y `wsym.py` (simboliza el wprof).

## 2026-11-12 — pantalla de brillo; los jobs SPU se quedan sin hilos
- El video de intro (`SMLOGO.M2V`) termina (`EndSeq` + `Close`) y aparece la **pantalla de calibración de brillo**, bien dibujada.
- Se cuelga ahí: los 128 ids de tareas del taskset de partículas se agotaban porque `pthread_create` falla y el id no se liberaba. Arreglado con una cola diferida, pero los jobs encolados no corren: el fallo es ENOMEM al reservar stacks de 16 MB con 6 hilos vivos.
- **Próximo:** pool fijo de workers SPU, stacks más chicos, medir el heap libre; `cellSpursJoinTask2`; input.

## 2026-11-12 — video de intro sin FFmpeg
- `cellVdec` sin FFmpeg nunca llenaba `CellVdecMpeg2Info` (frame rate 0, pts -1): el reproductor no terminaba nunca. Ahora saca el frame rate del header de secuencia MPEG-2, sintetiza los pts y usa una cola de 32 imágenes. Los cuadros se ven en negro (falta decodificar de verdad).

## 2026-11-12 — imagen con los shaders del juego
- Backend Vulkan sobre **NVK** en la consola (swapchain 1280×720). glslang 11 compilado para Switch: los shaders del RSX van HLSL → SPIR-V en la consola, con caché en la SD. 32 pipelines, 0 fallas.
- Se ven los logos y los créditos del estudio a ~45 FPS. Desde el video baja a ~3,8 FPS.

## 2026-11-12 — 10 minutos estable con RSX null
- El parking del FIFO del RSX descartaba los command buffers de EDGE que todavía llenaba el SPU (gracia de 50 ms). Con más gracia y semáforos de acquire, flipea estable a ~4,4 FPS durante 9 min.
- Edge zlib, SPURS, PM de WWS y fios funcionando. Título leído de `PARAM.SFO` (antes caía en un placeholder).

## 2026-11-11 — primer arranque en consola
- Lift completo del PPU (con stubs HLE) y de las SPU: 8 ELF, el policy module de WWS y 19 jobs relocables.
- Runtime de ps3recomp compilado para AArch64 con libnx: memoria guest de 4 GB, puente de excepciones a la tabla de señales, compat POSIX.

## 2026-11-11 — elección de ruta
- Se descartó la versión de PS4 (no hay binarios ARM64; FEX/JIT no es viable en Horizon) y se eligió **recompilación estática de la versión de PS3** con ps3recomp.
