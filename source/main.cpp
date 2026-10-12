#include <time.h> // Correção para o bug do nanosleep na OpenOrbis
#include "proxy.h"
#include "log.h"

#include <stdio.h>
#include <unistd.h>

// Declaração direta para evitar problemas com os headers da SDK
extern "C" int sceNetInit(void);
extern "C" int sceSysmoduleLoadModule(unsigned int id);
// ATENCAO: IDs escritos de memoria. Confira em orbis/Sysmodule.h do seu toolchain.
#define SCE_SYSMODULE_NET  0x0094
#define SCE_SYSMODULE_HTTP 0x00E4
#define SCE_SYSMODULE_SSL  0x00E5

#ifndef LOCAL_PORT
#define LOCAL_PORT 8080
#endif

int main() {
    gt_log("[gtstore] main iniciou\n");

#ifdef GT_MINIMAL
    // Build de teste de empacotamento: nao toca em rede nenhuma.
    gt_log("[gtstore] modo MINIMAL\n");
    while (true) {
        sleep(10);
    }
#else
    // 1. Carregar modulos de sistema necessarios para o proxy
    gt_log("[gtstore] sysmodule NET=%d HTTP=%d SSL=%d\n",
           sceSysmoduleLoadModule(SCE_SYSMODULE_NET),
           sceSysmoduleLoadModule(SCE_SYSMODULE_HTTP),
           sceSysmoduleLoadModule(SCE_SYSMODULE_SSL));

    // 2. Inicializar a pilha de rede (pre-requisito de sceNetPoolCreate/sceHttpInit)
    int nrc = sceNetInit();
    gt_log("[gtstore] sceNetInit=0x%08x\n", (unsigned)nrc);

    // 3. Iniciar o proxy
    if (!gtproxy::start(LOCAL_PORT)) {
        gt_log("[gtstore] proxy nao iniciou\n");
        // Nao encerra: fica vivo para o erro nao virar tela de crash.
    } else {
        gt_log("[gtstore] proxy ativo na porta %u\n", (unsigned)gtproxy::port());
    }

    // Mantem a aplicacao viva e o tunel aberto
    while (true) {
        sleep(10);
    }
#endif
    return 0;
}
