<h1 align=center>God of War III — native Nintendo Switch port</h1>

**English** | [Español](README.es.md)

A native Nintendo Switch port of the PS3 version of **God of War III** (BCUS-98111).

It is not an emulator. The PPU (PowerPC) and SPU code in the game's `EBOOT.BIN` is statically recompiled to C/C++ with
[ps3recomp](https://github.com/sp00nznet/ps3recomp), built for the Switch with devkitPro/libnx, and the game is drawn
by a Vulkan backend running on NVK (Mesa). RSX shaders are translated to SPIR-V on the console.

> [!NOTE]
> This repository contains **no game data and no code derived from the game**. You need your own copy of the PS3
> disc and a decrypted `EBOOT.BIN`.

## Status

**Work in progress, not playable yet. Overall progress: ~20%.**

```text
Material & analysis      ██████████  100 %
Boot (boot → menu)       ██████░░░░   60 %
Graphics (RSX → Vulkan)  ███▌░░░░░░   35 %
Switch layer (Horizon)   █████▌░░░░   55 %
Performance (30 FPS)     ▌░░░░░░░░░    5 %
Full game                ░░░░░░░░░░    0 %
Full translation         ░░░░░░░░░░    0 %
```

On the console, at stock clocks with no overclock, it boots, reads the PSARCs, runs SPURS and the SPU jobs, shows the
studio logos and credits (~45 FPS), plays the intro video and draws the **brightness calibration screen** with the
game's own shaders (32 of 32 pipelines OK).

**Where it is stuck:** from the video on, it drops to ~4 FPS and then nearly stalls. Some SPU jobs never run because
thread creation runs out of memory (ENOMEM with 6 threads using 16 MB stacks). The pad does not get past the
brightness screen yet.

**Goals:**
- **30 FPS without overclock.** This is the hardest one: the game leans heavily on all 6 SPUs, which run on 3 A57
  cores on the Switch. The plan is to move MLAA from the SPUs to a shader AA pass, rewrite the hottest jobs with
  NEON, add dynamic resolution and cut post effects (DoF, motion blur).
- **Full Spanish translation.** The US disc only ships `ENGLISH.PSARC`. It needs the language PSARC from a European or
  Latin American release (your own dump) or a text translation, plus fonts with á, ñ, ¿ and ¡.

The detailed log is in [PORT_STATUS.md](PORT_STATUS.md) (newest first, in Spanish) and the phase plan is in
[docs/ROADMAP.md](docs/ROADMAP.md).

## Installing on the Switch

There is no playable release yet. The NRO and instructions will be published once it reaches the menu. Current test
setup:

```text
/switch/gow3-nx/
  gow3-nx.nro
  PS3_GAME/PARAM.SFO
  USRDIR/            (the PSARCs, CINEMATICS/ and SOUNDS/ from your disc)
```

Run it in full application mode (hold **R** while opening an installed game, or use a forwarder). Applet mode
(Album) does not give it enough memory.

## Layout

| Path | What it is |
|---|---|
| `port/` | The port's own code: `main.cpp` (boot and title environment), extra HLE (`lfqueue.cpp`, `stubs.cpp`) and the game's CMake |
| `nx/` | Horizon layer: 4 GB guest memory, exception bridge, POSIX compat, glslang shims |
| `patches/` | Switch changes on top of ps3recomp `de4f090` (Vulkan/NVK, cellVdec without FFmpeg, SPU job queue, FS) |
| `tools/lift_reloc_jobmods.py` | Lifts the 19 relocatable WWS SPU jobs |
| `docs/ROADMAP.md` | Phase plan (Spanish) |

## Building (summary)

1. Install devkitPro (devkitA64 + libnx + switch-sdl2) and get the NVK SDK for Switch.
2. Clone ps3recomp at `de4f090` and apply `patches/ps3recomp-switch.patch`.
3. Decrypt your `EBOOT.BIN` (RPCS3: Utilities → Decrypt PS3 Binaries) and run the ps3recomp pipeline
   (output goes to `port/ppu/` and `port/spu/`, which are never committed).
4. Configure and build `port/` with the Switch toolchain.

## Credits

- [ps3recomp](https://github.com/sp00nznet/ps3recomp) (MIT): lifters, runtime and HLE.
- [gow2-recomp](https://github.com/andrebrumdev/gow2-recomp) and [Ascension-R](https://github.com/rigaz29/Ascension-R),
  references for the same Santa Monica Studio engine.
- [RPCS3](https://github.com/RPCS3/rpcs3), devkitPro, libnx, glslang and Mesa/NVK.

God of War is a property of Sony Interactive Entertainment. Non-commercial project.

## Support

If you like my work and want to support me:

<a href="https://tecito.app/tebit"><img src="https://img.shields.io/badge/Invitame_un-tecito-6d4aff?style=for-the-badge" alt="Invitame un tecito" height="32"></a>
