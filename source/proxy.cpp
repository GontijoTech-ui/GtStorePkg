#include "proxy.h"
#include "upstream.h"
#include "log.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

// --- INJEÇÃO PARA CORRIGIR BUGS DA OPENORBIS ---
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 4
#endif

extern "C" {
    struct timespec;
    int usleep(unsigned int usec);
    int clock_gettime(int clock_id, struct timespec *tp);
}
// -----------------------------------------------

#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

// Nunca registre a URL real: ela carrega o token.
#ifndef GTPROXY_LOG
#define GTPROXY_LOG(...) gt_log("[gtproxy] " __VA_ARGS__)
#endif

namespace {

const size_t kMaxRequestBytes = 8192;
const size_t kMaxEntries = 64;
const size_t kBufSize = 64 * 1024;
const int kMaxConnections = 8;
const int kClientReadTimeoutSec = 15;
const int kClientWriteTimeoutSec = 60;

pthread_mutex_t g_mu = PTHREAD_MUTEX_INITIALIZER;
std::map<std::string, std::string> g_urls;  // id -> URL real
std::deque<std::string> g_order;            // ordem de insercao
unsigned long long g_rng = 0;
unsigned short g_port = 0;
int g_server_fd = -1;
bool g_running = false;
bool g_started = false;
pthread_t g_accept_thread;
int g_active = 0;

class Lock {
public:
    explicit Lock(pthread_mutex_t* m) : m_(m) { pthread_mutex_lock(m_); }
    ~Lock() { pthread_mutex_unlock(m_); }

private:
    Lock(const Lock&);
    Lock& operator=(const Lock&);
    pthread_mutex_t* m_;
};

// ---------------------------------------------------------------- utilitarios

std::string num(long long v) {
    char b[32];
    snprintf(b, sizeof(b), "%lld", v);
    return b;
}

// Remove CR/LF e controles de valores vindos do upstream (evita injecao de cabecalho).
std::string clean(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = (unsigned char)s[i];
        if (c >= 0x20 && c != 0x7f) out += (char)c;
    }
    return out;
}

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t')) --b;
    return s.substr(a, b - a);
}

const char* reason_phrase(int s) {
    switch (s) {
        case 200: return "OK";
        case 206: return "Partial Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 304: return "Not Modified";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 416: return "Range Not Satisfiable";
        case 431: return "Request Header Fields Too Large";
        case 500: return "Internal Server Error";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        case 504: return "Gateway Timeout";
        default: return "Status";
    }
}

bool send_all(int fd, const char* data, size_t len) {
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

bool send_all(int fd, const std::string& s) { return send_all(fd, s.data(), s.size()); }

void send_simple(int fd, int status, const std::string& body, bool head,
                 const char* content_type = "text/plain; charset=utf-8",
                 const char* extra = "") {
    std::string h = "HTTP/1.1 " + num(status) + " " + reason_phrase(status) + "\r\n";
    h += std::string("Content-Type: ") + content_type + "\r\n";
    h += "Content-Length: " + num((long long)body.size()) + "\r\n";
    h += extra;
    h += "Connection: close\r\n\r\n";
    if (!head) h += body;
    send_all(fd, h);
}

// "bytes 0-99/1000" -> 1000; "bytes */1000" -> 1000; "*" ou ausente -> -1.
long long content_range_total(const std::string& cr) {
    size_t slash = cr.rfind('/');
    if (slash == std::string::npos) return -1;
    const char* p = cr.c_str() + slash + 1;
    if (*p < '0' || *p > '9') return -1;
    return strtoll(p, NULL, 10);
}

// Aceita um unico intervalo: bytes=A-B, bytes=A- ou bytes=-N.
bool valid_range(const std::string& v) {
    if (v.compare(0, 6, "bytes=") != 0) return false;
    size_t i = 6, a = 0, b = 0;
    while (i < v.size() && v[i] >= '0' && v[i] <= '9') { ++i; ++a; }
    if (i >= v.size() || v[i] != '-') return false;
    ++i;
    while (i < v.size() && v[i] >= '0' && v[i] <= '9') { ++i; ++b; }
    return i == v.size() && (a + b) > 0 && a <= 18 && b <= 18;
}

bool valid_id(const std::string& id) {
    if (id.size() != 16) return false;
    for (size_t i = 0; i < id.size(); ++i) {
        char c = id[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}

std::string sanitize_name(const std::string& n) {
    std::string out;
    for (size_t i = 0; i < n.size() && out.size() < 64; ++i) {
        char c = n[i];
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                  c == '.' || c == '_' || c == '-';
        out += ok ? c : '_';
    }
    return out;
}

// xorshift64*; chamar com g_mu travado. Os ids so valem em loopback, nao e criptografia.
unsigned long long next_rand() {
    if (g_rng == 0) {
        timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        g_rng = ((unsigned long long)ts.tv_sec << 32) ^ (unsigned long long)ts.tv_nsec ^
                0x9E3779B97F4A7C15ULL;
    }
    g_rng ^= g_rng >> 12;
    g_rng ^= g_rng << 25;
    g_rng ^= g_rng >> 27;
    return g_rng * 2685821657736338717ULL;
}

bool lookup(const std::string& id, std::string& url) {
    Lock l(&g_mu);
    std::map<std::string, std::string>::const_iterator it = g_urls.find(id);
    if (it == g_urls.end()) return false;
    url = it->second;
    return true;
}

// -------------------------------------------------------------- pedido HTTP

struct Request {
    std::string method;
    std::string path;
    std::string range;
};

// 0 = ok; >0 = status HTTP de erro; -1 = descartar a conexao sem responder.
int read_request(int fd, Request& req) {
    std::string data;
    char tmp[1024];
    while (data.find("\r\n\r\n") == std::string::npos) {
        if (data.size() > kMaxRequestBytes) return 431;
        ssize_t n = recv(fd, tmp, sizeof(tmp), 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;  // inclui timeout
        }
        if (n == 0) return -1;
        data.append(tmp, (size_t)n);
    }

    size_t hdr_end = data.find("\r\n\r\n");
    size_t line_end = data.find("\r\n");
    std::string line = data.substr(0, line_end);

    size_t sp1 = line.find(' ');
    size_t sp2 = (sp1 == std::string::npos) ? std::string::npos : line.find(' ', sp1 + 1);
    if (sp1 == std::string::npos || sp2 == std::string::npos) return 400;
    if (line.compare(sp2 + 1, 7, "HTTP/1.") != 0) return 400;

    req.method = line.substr(0, sp1);
    std::string target = line.substr(sp1 + 1, sp2 - sp1 - 1);
    size_t q = target.find('?');
    req.path = (q == std::string::npos) ? target : target.substr(0, q);

    size_t pos = line_end + 2;
    while (pos <= hdr_end) {
        size_t e = data.find("\r\n", pos);
        std::string h = data.substr(pos, e - pos);
        pos = e + 2;
        if (strncasecmp(h.c_str(), "range:", 6) == 0) req.range = trim(h.substr(6));
    }
    return 0;
}

// ------------------------------------------------------------------- download

void serve_download(int fd, bool head, const std::string& url, const std::string& range,
                    const std::string& id) {
    std::unique_ptr<Upstream> up(upstream_create());
    if (!up) {
        send_simple(fd, 502, "upstream indisponivel", head);
        return;
    }

    // URLs assinadas costumam valer so para GET: um HEAD vira um GET de 1 byte,
    // e o tamanho total sai do Content-Range.
    UpstreamHead uh;
    if (!up->open(url, head ? std::string("bytes=0-0") : range, uh)) {
        GTPROXY_LOG("%s: falha ao contatar o upstream\n", id.c_str());
        send_simple(fd, 502, "falha ao contatar o servidor", head);
        return;
    }

    int status = uh.status;
    long long length = uh.content_length;
    std::string content_range = clean(uh.content_range);

    if (head) {
        long long total = content_range_total(content_range);
        if ((status == 206 || status == 416) && total >= 0) {
            status = 200;
            length = total;
            content_range.clear();
        } else if (status == 206) {
            status = 200;
            length = -1;
            content_range.clear();
        }
    }

    GTPROXY_LOG("%s: %s -> %d len=%lld\n", id.c_str(), head ? "HEAD" : "GET", status, length);

    std::string type = clean(uh.content_type);
    if (type.empty()) type = "application/octet-stream";

    std::string h = "HTTP/1.1 " + num(status) + " " + reason_phrase(status) + "\r\n";
    h += "Content-Type: " + type + "\r\n";
    if (length >= 0) h += "Content-Length: " + num(length) + "\r\n";
    if (!content_range.empty()) h += "Content-Range: " + content_range + "\r\n";
    h += "Accept-Ranges: bytes\r\nConnection: close\r\n\r\n";
    if (!send_all(fd, h)) return;
    if (head) return;

    std::vector<char> buf(kBufSize);
    for (;;) {
        long n = up->read(&buf[0], buf.size());
        if (n < 0) {
            GTPROXY_LOG("%s: erro lendo do upstream\n", id.c_str());
            break;  // fecha a conexao: o cliente ve o corpo truncado como erro
        }
        if (n == 0) break;
        if (!send_all(fd, &buf[0], (size_t)n)) break;  // cliente saiu
    }
    up->close();
}

void handle_client(int fd) {
    Request req;
    int rc = read_request(fd, req);
    if (rc < 0) return;
    if (rc > 0) {
        send_simple(fd, rc, "pedido invalido", false);
        return;
    }

    bool head = (req.method == "HEAD");
    if (req.method != "GET" && !head) {
        send_simple(fd, 405, "metodo nao permitido", false, "text/plain; charset=utf-8",
                    "Allow: GET, HEAD\r\n");
        return;
    }

    if (req.path == "/status") {
        send_simple(fd, 200, "{\"status\":\"active\"}", head, "application/json; charset=utf-8");
        return;
    }

    if (req.path.compare(0, 3, "/d/") != 0) {
        send_simple(fd, 404, "nao encontrado", head);
        return;
    }
    size_t id_end = req.path.find('/', 3);
    std::string id = req.path.substr(3, id_end == std::string::npos ? std::string::npos : id_end - 3);
    std::string url;
    if (!valid_id(id) || !lookup(id, url)) {
        send_simple(fd, 404, "nao encontrado", head);
        return;
    }

    serve_download(fd, head, url, valid_range(req.range) ? req.range : std::string(), id);
}

void* client_thread(void* arg) {
    int fd = (int)(intptr_t)arg;
    handle_client(fd);
    close(fd);
    Lock l(&g_mu);
    --g_active;
    return NULL;
}

void tune_client(int fd) {
    timeval rtv;
    rtv.tv_sec = kClientReadTimeoutSec;
    rtv.tv_usec = 0;
    timeval wtv;
    wtv.tv_sec = kClientWriteTimeoutSec;
    wtv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &rtv, sizeof(rtv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &wtv, sizeof(wtv));
#ifdef SO_NOSIGPIPE
    int on = 1;
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
#endif
}

void* accept_loop(void*) {
    for (;;) {
        bool running;
        {
            Lock l(&g_mu);
            running = g_running;
        }
        if (!running) break;

        pollfd pfd;
        pfd.fd = g_server_fd;
        pfd.events = POLLIN;
        pfd.revents = 0;
        int pr = poll(&pfd, 1, 500);
        if (pr < 0) {
            if (errno == EINTR) continue;
            perror("poll");
            usleep(100000);
            continue;
        }
        if (pr == 0) continue;

        int client = accept(g_server_fd, NULL, NULL);
        if (client < 0) {
            if (errno == EINTR || errno == ECONNABORTED || errno == EAGAIN) continue;
            perror("accept");
            usleep(100000);  // evita busy loop em erros persistentes (EMFILE, ENFILE...)
            continue;
        }
        tune_client(client);

        bool admitted = false;
        {
            Lock l(&g_mu);
            if (g_active < kMaxConnections) {
                ++g_active;
                admitted = true;
            }
        }
        if (!admitted) {
            send_simple(client, 503, "ocupado", false, "text/plain; charset=utf-8",
                        "Retry-After: 1\r\n");
            close(client);
            continue;
        }

        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        pthread_attr_setstacksize(&attr, 512 * 1024);
        pthread_t t;
        int rc = pthread_create(&t, &attr, client_thread, (void*)(intptr_t)client);
        pthread_attr_destroy(&attr);
        if (rc != 0) {
            GTPROXY_LOG("pthread_create falhou: %d\n", rc);
            {
                Lock l(&g_mu);
                --g_active;
            }
            send_simple(client, 503, "ocupado", false);
            close(client);
        }
    }
    return NULL;
}

void abort_start(int fd) {
    if (fd >= 0) close(fd);
    upstream_term();
}

}  // namespace

namespace gtproxy {

bool start(unsigned short port_) {
    {
        Lock l(&g_mu);
        if (g_started) return true;
    }
    if (!upstream_init()) {
        GTPROXY_LOG("upstream_init falhou\n");
        return false;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        abort_start(-1);
        return false;
    }

    int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt(SO_REUSEADDR)");  // nao fatal
    }

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  // so conexoes locais
    addr.sin_port = htons(port_);

    if (bind(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        abort_start(fd);
        return false;
    }
    if (listen(fd, 16) < 0) {
        perror("listen");
        abort_start(fd);
        return false;
    }
    socklen_t len = sizeof(addr);
    if (getsockname(fd, (sockaddr*)&addr, &len) < 0) {
        perror("getsockname");
        abort_start(fd);
        return false;
    }

    {
        Lock l(&g_mu);
        g_server_fd = fd;
        g_port = ntohs(addr.sin_port);
        g_running = true;
    }
    int rc = pthread_create(&g_accept_thread, NULL, accept_loop, NULL);
    if (rc != 0) {
        GTPROXY_LOG("pthread_create falhou: %d\n", rc);
        {
            Lock l(&g_mu);
            g_running = false;
            g_server_fd = -1;
            g_port = 0;
        }
        abort_start(fd);
        return false;
    }
    {
        Lock l(&g_mu);
        g_started = true;
    }
    GTPROXY_LOG("escutando em 127.0.0.1:%u\n", (unsigned)port());
    return true;
}

void stop() {
    {
        Lock l(&g_mu);
        if (!g_started) return;
        g_running = false;
    }
    pthread_join(g_accept_thread, NULL);
    close(g_server_fd);

    for (int i = 0; i < 50; ++i) {  // ate ~5 s para as conexoes ativas terminarem
        {
            Lock l(&g_mu);
            if (g_active == 0) break;
        }
        usleep(100000);
    }
    upstream_term();

    Lock l(&g_mu);
    g_server_fd = -1;
    g_port = 0;
    g_started = false;
}

unsigned short port() {
    Lock l(&g_mu);
    return g_port;
}

std::string register_url(const std::string& upstream_url, const std::string& file_name) {
    char id[17];
    unsigned short p;
    {
        Lock l(&g_mu);
        p = g_port;
        if (p == 0) return std::string();
        snprintf(id, sizeof(id), "%016llx", next_rand());
        g_urls[id] = upstream_url;
        g_order.push_back(id);
        while (g_order.size() > kMaxEntries) {
            g_urls.erase(g_order.front());
            g_order.pop_front();
        }
    }
    char prefix[64];
    snprintf(prefix, sizeof(prefix), "http://127.0.0.1:%u/d/", (unsigned)p);
    std::string out = std::string(prefix) + id;
    if (!file_name.empty()) out += "/" + sanitize_name(file_name);
    return out;
}

void unregister_url(const std::string& local_url) {
    size_t pos = local_url.find("/d/");
    if (pos == std::string::npos) return;
    std::string id = local_url.substr(pos + 3, 16);
    if (!valid_id(id)) return;

    Lock l(&g_mu);
    g_urls.erase(id);
    for (std::deque<std::string>::iterator it = g_order.begin(); it != g_order.end(); ++it) {
        if (*it == id) {
            g_order.erase(it);
            break;
        }
    }
}

}  // namespace gtproxy
