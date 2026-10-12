#include <stdio.h>
#include <unistd.h>

void test_log(const char* path, const char* msg) {
    FILE* f = fopen(path, "w");
    if (f) {
        fprintf(f, "%s\n", msg);
        fclose(f);
    }
}

int main() {
    // 1. Tenta escrever no /data/ (se o sandbox permitir)
    test_log("/data/gtproxy_vivo.log", "Passei pelo loader! Log no /data/ funcionou.");
    
    // 2. Tenta escrever no pendrive (se o sandbox barrar o /data/)
    test_log("/mnt/usb0/gtproxy_vivo.log", "Passei pelo loader! Log no USB0 funcionou.");
    test_log("/mnt/usb1/gtproxy_vivo.log", "Passei pelo loader! Log no USB1 funcionou.");

    // Mantém o processo vivo para o PS4 não fechar a tela
    while (1) {
        sleep(10);
    }
    return 0;
}
