#include <time.h> // Correção para o bug do nanosleep na OpenOrbis
#include "proxy.h"

#include <stdio.h>
#include <unistd.h>

// Declaração direta para evitar problemas com os headers da SDK
extern "C" int sceSysmoduleLoadModule(unsigned int id);
#define SCE_SYSMODULE_NET  0x0094
#define SCE_SYSMODULE_HTTP 0x00E4
#define SCE_SYSMODULE_SSL  0x00E5

#ifndef LOCAL_PORT
#define LOCAL_PORT 8080
#endif

int main() {
    // 1. Carregar modulos de sistema necessarios para o proxy
    sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    sceSysmoduleLoadModule(SCE_SYSMODULE_HTTP);
    sceSysmoduleLoadModule(SCE_SYSMODULE_SSL);

    // 2. Iniciar o proxy
    if (!gtproxy::start(LOCAL_PORT)) {
        fprintf(stderr, "proxy nao iniciou\n");
        return 1;
    }

    // Mantem a aplicacao viva e o tunel aberto
    while (true) {
        sleep(10);
    }
    
    return 0;
}
