/*
 * ps3recomp game project -- game-specific hooks and overrides.
 *
 * main.cpp is the runner and is game-agnostic. This file is where a port puts
 * everything that is not: the PRX loading hook it must answer, overrides for
 * firmware functions the HLE library gets wrong for this title, and patches
 * over recompiled code.
 *
 * It starts out as the empty version of all of that, which is what a game with
 * no lifted PRX and no overrides needs.
 */

#include <cstdio>
#include <cstring>

#include "ps3emu/ps3types.h"
#include "ps3emu/error_codes.h"

/* ---------------------------------------------------------------------------
 * PRX loading
 *
 * main.cpp calls this once, after the lifted function table is registered and
 * vm_base is live, before the game runs. A title that lifts a real system PRX
 * -- libsre, which is cellSpurs and cellSync -- loads its image into guest RAM
 * here and registers its exports. Most ports never need to: the HLE library
 * answers those modules instead.
 * -----------------------------------------------------------------------*/

extern "C" void gow3_lfqueue_register(void);   /* lfqueue.cpp, from Ascension-R */

extern "C" void ps3_load_prx_modules(void)
{
    gow3_lfqueue_register();
}

/* ---------------------------------------------------------------------------
 * Example: Override a specific NID
 *
 * The module system resolves function imports by NID.  To replace a default
 * HLE stub with custom logic, register your function at init time.
 * -----------------------------------------------------------------------*/

/*
 * Example: override cellGcmSetFlipMode (NID 0xA53D12AE) to force vsync.
 *
 *   static void my_cellGcmSetFlipMode(u32 mode)
 *   {
 *       printf("[hook] cellGcmSetFlipMode: forcing VSYNC (was %u)\n", mode);
 *       // Force vsync regardless of what the game requested
 *       cellGcmSetFlipMode(CELL_GCM_DISPLAY_VSYNC);
 *   }
 *
 * Then in your init code:
 *
 *   ps3::modules::override_nid(0xA53D12AE,
 *       reinterpret_cast<void*>(my_cellGcmSetFlipMode));
 */

/* ---------------------------------------------------------------------------
 * Example: Hook into module loading
 *
 * You can intercept cellSysmoduleLoadModule to perform custom actions
 * when a particular module is loaded.
 * -----------------------------------------------------------------------*/

/*
 *   static s32 my_loadmodule_hook(u16 id)
 *   {
 *       printf("[hook] Module 0x%04X loaded\n", id);
 *
 *       if (id == CELL_SYSMODULE_AUDIO) {
 *           // Perform custom audio backend initialization
 *           init_my_audio_backend();
 *       }
 *
 *       // Call the original implementation
 *       return cellSysmoduleLoadModule(id);
 *   }
 */

/* ---------------------------------------------------------------------------
 * Game-specific patches
 *
 * Sometimes you need to patch recompiled code at specific addresses.
 * Use the patch system to modify behavior without editing generated code.
 * -----------------------------------------------------------------------*/

/*
 *   // Skip an integrity check at guest address 0x00812FA0
 *   ps3::patches::nop_range(0x00812FA0, 0x00812FB0);
 *
 *   // Replace a function call with a direct return
 *   ps3::patches::force_return(0x009A1000, 0);  // return 0
 */

/* ---------------------------------------------------------------------------
 * Spin-wait hint (PPU_SPIN_HINT, inserted by tools/patch_spin_hint.py)
 *
 * GoW3's PPU busy-waits on job counters the SPUs clear (func_0022F1CC spins on
 * *(u16*)(r20+0x14) and +0x18). On Horizon a spinning thread never gives its
 * core to a same-priority thread, so the SPU worker that would clear the word
 * waits for a timeslice. Yield on every iteration of a pure spin.
 * -----------------------------------------------------------------------*/
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstdint>
#ifdef __SWITCH__
extern "C" void svcSleepThread(int64_t nano);
#else
#include <thread>
#endif

extern "C" void ppu_spin_hint(void)
{
    static std::atomic<int> s_first{0};
    if (s_first.load(std::memory_order_relaxed) == 0 && s_first.exchange(1) == 0)
        std::fprintf(stderr, "[spin] first PPU spin hint\n");
#ifdef __SWITCH__
    svcSleepThread(0);   /* yield to a same-priority thread on this core */
#else
    std::this_thread::yield();
#endif
}

extern "C" void ppu_wait_backoff(uint32_t* n)
{
    static std::atomic<int64_t> s_ns{-1};
    int64_t ns = s_ns.load(std::memory_order_relaxed);
    if (ns < 0) {
        const char* e = std::getenv("GOW3_WAIT_NS");
        ns = e ? std::atoll(e) : 50000;
        if (ns < 0) ns = 0;
        s_ns.store(ns, std::memory_order_relaxed);
        std::fprintf(stderr, "[spin] PPU job-counter wait backoff: %lld ns after 32 polls\n", (long long)ns);
    }
    if (ns == 0 || ++*n < 32) return;
#ifdef __SWITCH__
    svcSleepThread(ns);
#else
    std::this_thread::sleep_for(std::chrono::nanoseconds(ns));
#endif
}

/* ---------------------------------------------------------------------------
 * Probe for func_002E7B18 (session 31): "has the job queue reached seq want?"
 * = min(c[1..7]) >= want. Once a second print the last call that failed, how
 * many calls failed, and the slot(s) that were behind.
 * -----------------------------------------------------------------------*/
#include <chrono>
#include <cstdlib>
#include <mutex>
extern "C" void gow3_probe_jq(uint32_t p, uint32_t idx, uint32_t want, uint32_t lr, const uint16_t* c)
{
    static int on = -1; if (on < 0) { const char* e = getenv("GOW3_JQ"); on = e && *e == '1'; }
    if (!on) return;
    static std::mutex m;
    static uint64_t calls = 0, fails = 0, behind[8] = {0};
    static auto last = std::chrono::steady_clock::now();
    uint32_t mn = 0xFFFF; for (int i = 1; i < 8; i++) if (c[i] < mn) mn = c[i];
    std::lock_guard<std::mutex> g(m);
    calls++;
    if (mn < want) { fails++; for (int i = 1; i < 8; i++) if (c[i] < want) behind[i]++; }
    auto now = std::chrono::steady_clock::now();
    if (now - last < std::chrono::seconds(1)) return;
    last = now;
    std::fprintf(stderr, "[jq] calls=%llu fails=%llu p=%08X idx=%u want=%u lr=%08X c=%04X|%u %u %u %u %u %u %u behind=%llu %llu %llu %llu %llu %llu %llu\n",
        (unsigned long long)calls, (unsigned long long)fails, p, idx, want, lr,
        c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7],
        (unsigned long long)behind[1], (unsigned long long)behind[2], (unsigned long long)behind[3],
        (unsigned long long)behind[4], (unsigned long long)behind[5], (unsigned long long)behind[6],
        (unsigned long long)behind[7]);
    calls = fails = 0; for (int i = 0; i < 8; i++) behind[i] = 0;
}
