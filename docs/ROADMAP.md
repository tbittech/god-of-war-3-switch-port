# GoW3 (PS3) -> Switch — Roadmap

Ruta elegida: **recompilación estática** del juego de PS3. El código del PPU y de las SPU se traduce por adelantado a C/C++, se compila con devkitA64/libnx y sale un NRO. Los gráficos del RSX se traducen a deko3d. Sin emulador ni JIT.

## Bases que reusamos
| Pieza | De dónde | Para qué |
|---|---|---|
| Runtime + lifters PPU/SPU, HLE LV2/PRX, SPURS, cellGcm | `sp00nznet/ps3recomp` | Base del port: lifter, runtime y HLE |
| Pipeline de un juego SMS completo en arm64 (macOS/Metal) | `andrebrumdev/gow2-recomp` | Cómo resolvieron SPU, SPURS, FIOS y RSX FIFO. Ojo: GoW2 HD es de Bluepoint y no usa el engine de GoW3, así que sirve como guía de pipeline, no se copia tal cual |
| Ports de GoW Ascension (`rigaz29/Ascension-R`, `MaChInEgUn3/...`) | GitHub | Engine sucesor directo del de GoW3: el más parecido en jobs SPU y formatos |
| SpartanPort (GoW3 Remastered PS4) | `FelipePrado0/SpartanPort` | Referencia de lógica del juego y de bugs ya resueltos |
| Experiencia de los ports de 360 | `kof2002um-nx`, `PsiOps` | Capa Horizon, renderer deko3d, packs de assets, autotest (`devkitnx`) |

Antes de reusar código hay que revisar la licencia de cada repo.

## Fases

### F0 — Material y entorno
- [ ] Dump propio del disco (ver "Qué necesito" abajo)
- [ ] Desencriptar `EBOOT.BIN` -> `EBOOT.ELF` (RPCS3 o la herramienta `unself` de ps3recomp)
- [ ] Clonar ps3recomp y gow2-recomp en `ref/` y leer su pipeline
- [ ] Anotar región, versión y hash del ELF en ESTADO.md

### F1 — Análisis estático
- [ ] Parsear el ELF: segmentos, imports PRX (lista de funciones de sistema que hay que implementar con HLE)
- [ ] Extraer las imágenes SPU (`extract_spu_images.py`) e inventariarlas: cuántas son, cuáles son SPURS kernel/policy y cuáles son jobs (MLAA, skinning, culling, audio)
- [ ] Detectar los límites de funciones del PPU y exportar a Ghidra para las dudas puntuales
- **Sale cuando:** hay inventario de PRX y SPU con tamaños en ESTADO.md

### F2 — Arranque en PC (host x64 o Linux, sin Switch todavía)
- [ ] Lift del PPU a C++ y build con el runtime de ps3recomp, renderer null
- [ ] Llegar a `main`, cargar archivos del USRDIR, ver logs
- [ ] Lift de las SPU y arranque de SPURS
- **Sale cuando:** el juego pasa los menús en headless sin crashear

### F3 — Gráficos en PC
- [ ] Backend Vulkan de ps3recomp con el FIFO real del RSX
- [ ] Shaders RSX (fragment/vertex programs) -> SPIR-V/GLSL
- **Sale cuando:** se ve el menú y se llega a la primera escena jugable en PC

### F4 — Llevarlo a Switch
- [ ] Toolchain devkitA64: compilar el código generado más el runtime para AArch64
- [ ] Capa Horizon: memoria (mapear el espacio de 32 bits de PS3), hilos (PPU threads y SPU virtuales sobre 3 cores), archivos en romfs/SD, audio, input
- [ ] Backend **deko3d** del RSX (o mesa/GL como paso intermedio si deko3d frena)
- [ ] Deploy y autotest con `devkitnx`
- **Sale cuando:** el NRO arranca en consola y llega al menú

### F5 — Rendimiento (el cuello de botella real)
- [ ] Medir por subsistema: PPU, cada job SPU, GPU
- [ ] Jobs SPU calientes -> reescribirlos en C++ nativo con NEON, o pasarlos a GPU (MLAA -> FXAA/SMAA en shader)
- [ ] Bajar la resolución de render (720p dock / 540–600p portátil, con escalado dinámico)
- [ ] Bajar mips de texturas, sombras, partículas y LOD (conversor de assets offline, estilo `.nxpack`)
- [ ] Objetivo: 30 fps estables
- **Sale cuando:** el primer nivel es jugable entero en consola

### F6 — Juego completo
- [ ] Recorrer todo el juego con autotest y logs, arreglar crashes por zona
- [ ] Guardado, video (Bink/cutscenes), audio completo
- [ ] Opciones de calidad y empaquetado final

## Riesgos principales
1. **Throughput de las SPU en los A57:** 6 SPU contra 3 cores lentos. Es el riesgo #1 y lo mitiga F5.
2. **SPURS:** el HLE tiene que manejar bien la planificación y la sincronización. Se toma de ps3recomp y gow2-recomp.
3. **RSX -> deko3d:** mucho trabajo, pero ya tenemos experiencia con deko3d.
4. **Proyectos upstream que cambian rápido:** fijar el commit de ps3recomp que usemos.

## Qué necesito de vos
Tiene que ser **tu propio dump**, puesto en `GOD OF WAR/game/` (yo no leo esa carpeta entera, solo los archivos puntuales):
- La **carpeta del juego** extraída: `PS3_GAME/` con `USRDIR/EBOOT.BIN` y los datos. Sirve un ISO o una carpeta JB; si es ISO, lo extraemos.
- Si podés, el **EBOOT ya desencriptado** (`EBOOT.ELF`). RPCS3 lo hace con Utilities -> Decrypt PS3 Binaries, con tu firmware instalado.
- El **update** del juego (el PKG del último parche), si lo tenés: el ELF de la última versión es el que conviene usar.
- Tu región/ID (ej. BCUS98111 o BCES00510).
