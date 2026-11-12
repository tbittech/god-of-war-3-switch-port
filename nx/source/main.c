/*
 * gow3-nx: shell del port. Por ahora es una sonda de entorno que se prueba por
 * netloader con devkitnx. Cuando esté el runtime de ps3recomp, este main lo arranca.
 *
 * Marcas: boot -> probe_done. Sale sola con --autotest.
 */
#define AUTOTEST_IMPLEMENTATION
#include "autotest.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <switch.h>

#define DATA_DIR "sdmc:/switch/gow3-nx/USRDIR"

static void probe(void)
{
    u64 total = 0, used = 0;
    svcGetInfo(&total, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
    svcGetInfo(&used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0);
    at_log("mem total=%lluMB used=%lluMB applet=%d", (unsigned long long)(total >> 20),
           (unsigned long long)(used >> 20), (int)appletGetAppletType());

    /* PS3: 256 MB de main memory + 256 MB de VRAM local. Probamos reservarlos. */
    size_t sizes[] = { 256u << 20, 256u << 20, 256u << 20 };
    void *blk[3] = { 0 };
    for (int i = 0; i < 3; i++) {
        blk[i] = aligned_alloc(0x10000, sizes[i]);
        if (blk[i]) memset(blk[i], 0, sizes[i]);
        at_log("alloc %d (%zuMB): %s", i, sizes[i] >> 20, blk[i] ? "ok" : "FALLO");
    }
    for (int i = 0; i < 3; i++) free(blk[i]);

    /* Espacio virtual para mapear el address space de 32 bits de la guest (4 GB). */
    virtmemLock();
    void *va = virtmemFindAslr(0x100000000ull, 0x10000);
    virtmemUnlock();
    at_log("va 4GB libre: %p", va);

    struct stat st;
    at_log("datos %s: %s", DATA_DIR, stat(DATA_DIR, &st) == 0 ? "presentes" : "no estan");
}

int main(int argc, char **argv)
{
    at_init(argc, argv, 0);
    consoleInit(NULL);
    at_mark("boot");

    printf("gow3-nx sonda\n");
    probe();
    at_mark("probe_done");
    printf("listo. + para salir\n");

    PadState pad;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);
    while (appletMainLoop() && !at_should_quit()) {
        at_frame_begin();
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus) break;
        if (at_enabled() && at_frame() > 60) at_done(0);
        consoleUpdate(NULL);
        at_frame_end();
    }
    consoleExit(NULL);
    at_shutdown();
    return 0;
}
