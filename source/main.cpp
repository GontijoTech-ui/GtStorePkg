#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <iostream>

#define LOCAL_PORT 8080

// Função que roda em background gerindo a porta 8080
void start_background_proxy() {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) return;

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(LOCAL_PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        close(server_fd);
        return;
    }

    if (listen(server_fd, 10) < 0) {
        close(server_fd);
        return;
    }

    while (true) {
        client_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        if (client_socket < 0) continue;

        char buffer[2048] = {0};
        ssize_t bytesRead = read(client_socket, buffer, sizeof(buffer) - 1);
        
        if (bytesRead > 0) {
            // Resposta JSON padrão encaminhando para o seu ecossistema remoto/túnel
            const char* http_response = 
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/json; charset=utf-8\r\n"
                "Connection: close\r\n\r\n"
                "{\"status\":\"active\",\"target\":\"https://loja.gontijotech.com.br\"}";

            write(client_socket, http_response, strlen(http_response));
        }

        close(client_socket);
    }

    close(server_fd);
}

int main() {
    // Inicialização básica dos subsistemas da PS4
    // Dispara o proxy local em background na porta 8080
    std::thread proxy_thread(start_background_proxy);
    proxy_thread.detach();

    // Mantém a aplicação rodando em segundo plano sem travar a consola
    while (true) {
        sleep(10);
    }

    return 0;
}
