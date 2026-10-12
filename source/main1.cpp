// Servidor HTTP minimo, somente em loopback, que responde sempre com o mesmo JSON de status.
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifndef LOCAL_PORT
#define LOCAL_PORT 8080
#endif

static const int IO_TIMEOUT_SEC = 3;       // timeout de leitura e escrita por cliente
static const size_t REQUEST_MAX = 2048;    // limite do que lemos do cabecalho

static const char BODY[] = "{\"status\":\"active\",\"target\":\"https://seu-dominio.com\"}";

static volatile bool g_running = true;

// Envia tudo, tratando escritas parciais, EINTR e SIGPIPE.
static bool send_all(int fd, const char* data, size_t len) {
    int flags = 0;
#ifdef MSG_NOSIGNAL
    flags = MSG_NOSIGNAL;
#endif
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, data + sent, len - sent, flags);
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (n == 0) return false;
        sent += (size_t)n;
    }
    return true;
}

// Le ate o fim do cabecalho (\r\n\r\n), o buffer encher, o cliente fechar ou o timeout estourar.
static bool read_request(int fd) {
    char buf[REQUEST_MAX + 1];
    size_t used = 0;

    while (used < REQUEST_MAX) {
        ssize_t n = recv(fd, buf + used, REQUEST_MAX - used, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;  // inclui o timeout (EAGAIN/EWOULDBLOCK)
        }
        if (n == 0) return false;  // cliente fechou sem enviar o cabecalho completo

        used += (size_t)n;
        buf[used] = '\0';
        if (strstr(buf, "\r\n\r\n") != nullptr) return true;
    }
    return true;  // cabecalho maior que o buffer: responde mesmo assim
}

static void handle_client(int fd) {
    timeval tv;
    tv.tv_sec = IO_TIMEOUT_SEC;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#ifdef SO_NOSIGPIPE
    int on = 1;
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
#endif

    if (!read_request(fd)) return;

    char header[160];
    int hlen = snprintf(header, sizeof(header),
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: application/json; charset=utf-8\r\n"
                        "Content-Length: %zu\r\n"
                        "Connection: close\r\n\r\n",
                        sizeof(BODY) - 1);
    if (hlen <= 0 || (size_t)hlen >= sizeof(header)) return;

    if (!send_all(fd, header, (size_t)hlen)) return;
    send_all(fd, BODY, sizeof(BODY) - 1);
}

static void* start_background_proxy(void* arg) {
    (void)arg;

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return nullptr;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt(SO_REUSEADDR)");  // nao fatal
    }

    sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  // so aceita conexoes locais
    address.sin_port = htons(LOCAL_PORT);

    if (bind(server_fd, (sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind");
        close(server_fd);
        return nullptr;
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen");
        close(server_fd);
        return nullptr;
    }

    while (g_running) {
        int client_socket = accept(server_fd, nullptr, nullptr);
        if (client_socket < 0) {
            if (errno == EINTR || errno == ECONNABORTED) continue;
            perror("accept");
            usleep(100000);  // evita busy loop em erros persistentes (EMFILE, ENFILE...)
            continue;
        }

        handle_client(client_socket);
        close(client_socket);
    }

    close(server_fd);
    return nullptr;
}

int main() {
    pthread_t thread_id;
    int rc = pthread_create(&thread_id, nullptr, start_background_proxy, nullptr);
    if (rc != 0) {
        fprintf(stderr, "pthread_create falhou: %d\n", rc);
        return 1;
    }
    pthread_detach(thread_id);

    while (g_running) {
        sleep(10);
    }

    return 0;
}
