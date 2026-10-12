#include "proxy.h"
#include "upstream.h"

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
    int usleep(unsigned int usec);
}
// -----------------------------------------------

#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

// Nunca registre a URL real: ela carrega o token.
#ifndef GTPROXY_LOG
#define GTPROXY_LOG(...) fprintf(stderr, "[gtproxy] " __VA_ARGS__)
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
        if (!((c >= '
