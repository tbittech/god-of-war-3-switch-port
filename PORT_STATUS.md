# gow3-nx — registro del port

Lo más nuevo arriba. Todas las pruebas son en consola por nxlink, con los relojes de fábrica.

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
