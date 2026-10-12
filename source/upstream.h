// Interface do "lado de la" do proxy: busca a URL real (com token) e entrega o corpo.
#pragma once

#include <stddef.h>
#include <time.h> // Correcao do bug do nanosleep
#include <string>

struct UpstreamHead {
    int status;                  
    long long content_length;    
    std::string content_type;
    std::string content_range;   
    UpstreamHead() : status(0), content_length(-1) {}
};

class Upstream {
public:
    virtual ~Upstream() {}

    virtual bool open(const std::string& url, const std::string& range, UpstreamHead& head) = 0;
    virtual long read(void* buf, size_t len) = 0;
    virtual void close() = 0;
};

bool upstream_init();
void upstream_term();
Upstream* upstream_create();
