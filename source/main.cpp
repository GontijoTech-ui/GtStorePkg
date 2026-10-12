#include <stdio.h>
#include <unistd.h>

int main() {
    // Escreve um log básico para provar que a app abriu
    FILE* f = fopen("/data/hello_ps4.log", "w");
    if(f) {
        fprintf(f, "Vitoria! O empacotamento funcionou e o PS4 aceitou o PKG.\n");
        fclose(f);
    }

    // Fica em loop para a app não fechar imediatamente
    while (1) {
        sleep(10);
    }
    return 0;
}
