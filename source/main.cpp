#include <time.h>
#include "proxy.h"
#include "log.h"

#include <stdio.h>
#include <unistd.h>

extern "C" int sceNetInit(void);
extern "C" int sceSysmoduleLoadModule(unsigned int id);
// ATENCAO: IDs de memoria. Confira em orbis/Sysmodule.h do toolchain.
#define SCE_SYSMODULE_NET  0x0094
#define SCE_SYSMODULE_HTTP 0x00E4
#define SCE_SYSMODULE_SSL  0x00E5

#ifndef LOCAL_PORT
#define LOCAL_PORT 8080
#endif

int main() {
    gt_log("[gtstore] main iniciou\n");

#ifdef GT_MINIMAL
    gt_log("[gtstore] modo MINIMAL\n");
    while (true) {
        sleep(10);
    }
#else
    gt_log("[gtstore] sysmodule NET=%d HTTP=%d SSL=%d\n",
           sceSysmoduleLoadModule(SCE_SYSMODULE_NET),
           sceSysmoduleLoadModule(SCE_SYSMODULE_HTTP),
           sceSysmoduleLoadModule(SCE_SYSMODULE_SSL));

    int nrc = sceNetInit();
    gt_log("[gtstore] sceNetInit=0x%08x\n", (unsigned)nrc);

    if (!gtproxy::start(LOCAL_PORT)) {
        gt_log("[gtstore] proxy nao iniciou\n");
    } else {
        gt_log("[gtstore] proxy ativo na porta %u\n", (unsigned)gtproxy::port());
    }

    while (true) {
        sleep(10);
    }
#endif
    return 0;
}
