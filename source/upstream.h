// Interface do "lado de la" do proxy: busca a URL real (com token) e entrega o corpo.
// Backends: upstream_sce.cpp (PS4, libSceHttp) e tests/upstream_plain.cpp (testes no Linux).
#pragma once

#include <stddef.h>
#include <time.h>

// Injecao forcada do nanosleep para corrigir o bug da OpenOrbis
extern "C" {
    struct timespec;
    int nanosleep(const struct timespec *req, struct timespec *rem);
}

#include <string>

struct UpstreamHead {
    int status;                  // 200, 206, 403, 416...
    long long content_length;    // -1 = desconhecido (ex.: chunked)
    std::string content_type;
    std::string content_range;   // ex.: "bytes 0-99/1000"
    UpstreamHead() : status(0), content_length(-1) {}
};

class Upstream {
public:
    virtual ~Upstream() {}

    // Faz um GET em `url`. `range` e o valor do header Range ("" = sem Range).
    // Retorna false em falha de rede/TLS; senao `head` fica preenchido.
    virtual bool open(const std::string& url, const std::string& range, UpstreamHead& head) = 0;

    // Le o corpo: >0 bytes lidos, 0 = fim, <0 = erro.
    virtual long read(void* buf, size_t len) = 0;

    virtual void close() = 0;
};

bool upstream_init();
void upstream_term();
Upstream* upstream_create();  // o chamador faz delete
