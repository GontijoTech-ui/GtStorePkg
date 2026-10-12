#include <stdio.h>
#include <unistd.h>
#include <string.h>

// Assinaturas do seu proxy
namespace gtproxy {
    bool start(unsigned short port);
    void stop();
}

// Funções do sistema da Sony necessárias para a rede
extern "C" int sceNetInit(void);
extern "C" int sceSysmoduleLoadModule(int id);

#define SCE_SYSMODULE_NET  0x00E1
#define SCE_SYSMODULE_HTTP 0x00E2
#define SCE_SYSMODULE_SSL  0x00E3

void log_msg(const char* msg) {
    FILE* f = fopen("/data/gtproxy_main.log", "a");
    if(f) {
        fprintf(f, "%s\n", msg);
        fclose(f);
    }
}

int main() {
    log_msg("====================================");
    log_msg("GTSTORE: Passou pelo segurança do OS!");
    
    // 1. Carrega os módulos de sistema exigidos pelo upstream_sce
    log_msg("Carregando módulos da Sony...");
    sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    sceSysmoduleLoadModule(SCE_SYSMODULE_SSL);
    sceSysmoduleLoadModule(SCE_SYSMODULE_HTTP);

    // 2. Inicializa o stack de rede
    int net_status = sceNetInit();
    if (net_status == 0) {
        log_msg("Rede inicializada com sucesso!");
    } else {
        FILE* f = fopen("/data/gtproxy_main.log", "a");
        if(f) {
            fprintf(f, "Aviso: sceNetInit falhou com erro: %d\n", net_status);
            fclose(f);
        }
    }

    // 3. Inicia o túnel
    log_msg("Iniciando o servidor GTProxy na porta 8080...");
    if (gtproxy::start(8080)) {
        log_msg("SUCESSO: Proxy rodando e tunelando para loja.gontijotech.com.br!");
    } else {
        log_msg("ERRO CRÍTICO: Falha ao iniciar o Proxy na porta 8080.");
    }

    // 4. Mantém a aplicação viva para não ser fechada pelo sistema
    while (1) {
        sleep(10);
    }

    gtproxy::stop();
    return 0;
}
