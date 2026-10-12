// Inicia o proxy local. O resto do app usa gtproxy::register_url() (veja proxy.h).
#include "proxy.h"

#include <stdio.h>
#include <unistd.h>

#ifndef LOCAL_PORT
#define LOCAL_PORT 8080
#endif

int main() {
    // Antes de iniciar: sceNetInit() e o carregamento dos modulos de sistema Net/Ssl/Http,
    // se o app ainda nao fez isso.
    if (!gtproxy::start(LOCAL_PORT)) {
        fprintf(stderr, "proxy nao iniciou\n");
        return 1;
    }

    // Exemplo, no ponto em que o app inicia uma instalacao:
    //   std::string local = gtproxy::register_url(signed_url, "game.pkg");
    //   // entregue `local` ao instalador de pacotes no lugar de `signed_url`

    while (true) {
        sleep(10);
    }
}
