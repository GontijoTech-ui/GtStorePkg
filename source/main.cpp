// Inicia o proxy local. O resto do app usa gtproxy::register_url() (veja proxy.h).
#include "proxy.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

// Cabecalhos do sistema para iniciar a rede
#include <orbis/Sysmodule.h>
#include <orbis/Net.h>

#ifndef LOCAL_PORT
#define LOCAL_PORT 8080
#endif

int main() {
    // 1. Carregar modulos de sistema necessarios para o proxy
    sceSysmoduleLoadModule(ORBIS_SYSMODULE_NET);
    sceSysmoduleLoadModule(ORBIS_SYSMODULE_HTTP);
    sceSysmoduleLoadModule(ORBIS_SYSMODULE_SSL);

    // 2. Inicializar a memoria da placa de rede
    OrbisNetInitParam netParam;
    memset(&netParam, 0, sizeof(netParam));
    netParam.memory = malloc(1 * 1024 * 1024); // 1MB para buffer
    netParam.size = 1 * 1024 * 1024;
    netParam.flags = 0;
    sceNetInit(&netParam);

    // 3. Iniciar o proxy
    if (!gtproxy::start(LOCAL_PORT)) {
        fprintf(stderr, "proxy nao iniciou\n");
        return 1;
    }

    // Exemplo, no ponto em que o app inicia uma instalacao:
    //   std::string local = gtproxy::register_url(signed_url, "game.pkg");
    //   // entregue `local` ao instalador de pacotes no lugar de `signed_url`

    // Mantem a aplicacao viva
    while (true) {
        sleep(10);
    }
    
    return 0;
}
