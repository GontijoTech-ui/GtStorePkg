#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <pthread.h>

// Cabeçalhos do sistema PS4 / OpenOrbis
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <orbis/libkernel.h>
#include <orbis/Sysmodule.h>
#include <orbis/Net.h>

// Declaração externa do seu servidor proxy (implementado em proxy.cpp)
extern "C" {
    int start_gt_proxy_server(int port);
    void stop_gt_proxy_server();
}

// Função nativa da OrbisOS para interagir com o sistema e lançar URLs/Apps
extern "C" int sceSystemServiceLoadExec(const char *uri, const char *args[]);

void launch_ps4_browser(const char* url) {
    printf("[gtstore] A abrir navegador da PS4 em: %s\n", url);
    
    // O OrbisOS aceita disparar o browser nativo passando o URL como argumento de sistema ou URI Scheme
    const char *args[] = { url, nullptr };
    
    // Tenta invocar o navegador do sistema (CUSA00000 ou rotina de webapp nativa)
    // Se o URI direto falhar no firmware específico, o fallback abre a store web interna
    int ret = sceSystemServiceLoadExec("capp:eboot", args);
    if (ret < 0) {
        printf("[gtstore] Aviso: sceSystemServiceLoadExec retornou 0x%X\n", ret);
    }
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    printf("[gtstore] main iniciou\n");
    printf("[gtstore] modo AUTÓNOMO (Proxy + UI Nativa)\n");

    // 1. Inicializa módulos de rede do sistema PS4
    int ret = sceSysmoduleLoadModule(ORBIS_SYSMODULE_NET);
    if (ret < 0) {
        printf("[gtstore] Erro ao carregar o módulo NET: 0x%X\n", ret);
    } else {
        OrbisNetInitParam netInitParam;
        netInitParam.memory = malloc(1024 * 1024); // 1MB heap para a pilha de rede
        netInitParam.size = 1024 * 1024;
        sceNetInit(&netInitParam);
        printf("[gtstore] sceNetInit executado com sucesso.\n");
    }

    // 2. Inicia o servidor proxy local na porta 8080 em uma thread separada
    // para não bloquear o thread principal de inicialização da UI
    pthread_t proxy_thread;
    int proxy_port = 8080;
    
    int thread_create_res = pthread_create(&proxy_thread, nullptr, [](void* arg) -> void* {
        int port = *static_cast<int*>(arg);
        printf("[gtproxy] a iniciar servidor na porta %d...\n", port);
        start_gt_proxy_server(port);
        return nullptr;
    }, &proxy_port);

    if (thread_create_res != 0) {
        printf("[gtstore] Erro fatal ao criar a thread do proxy.\n");
        return -1;
    }

    // Dá um breve respiro de 1 segundo para garantir que o socket HTTP bindou na porta 8080
    sleep(1);
    printf("[gtstore] proxy ativo na porta 8080\n");

    // 3. Dispara a interface gráfica nativa apontando para o nosso servidor local
    // O proxy na porta 8080 encarrega-se de servir a página principal (loja.gontijotech.com.br)
    // e gerir o streaming dos pacotes de forma transparente.
    launch_ps4_browser("http://127.0.0.1:8080/");

    // Mantém o processo principal vivo para o servidor proxy continuar a processar os pedidos
    while (true) {
        sleep(10);
    }

    // Limpeza (caso o loop seja interrompido)
    stop_gt_proxy_server();
    sceNetTerm();
    sceSysmoduleUnloadModule(ORBIS_SYSMODULE_NET);

    return 0;
}
