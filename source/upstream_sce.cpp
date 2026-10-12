// Backend do PS4: busca a URL real com libSceHttp (HTTPS com verificacao de certificado).
//
// NAO testado no console. Pre-requisitos: sceNetInit() ja chamado e os modulos de
// sistema Net/Ssl/Http carregados (sceSysmoduleLoadModule*), se o seu app ainda nao
// faz isso. A verificacao de certificado fica LIGADA de proposito: a URL carrega um
// token, entao nao a desative.
#include "upstream.h"
#include "sce_http_decl.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

namespace {

const size_t kNetPoolSize = 256 * 1024;
const size_t kSslPoolSize = 512 * 1024;
const size_t kHttpPoolSize = 256 * 1024;
const uint32_t kConnectTimeoutUs = 15u * 1000u * 1000u;
const uint32_t kIoTimeoutUs = 30u * 1000u * 1000u;

int g_net_pool = -1;
int g_ssl_ctx = -1;
int g_http_ctx = -1;
int g_tmpl = -1;

void log_err(const char* what, int rc) {
    fprintf(stderr, "[gtproxy] %s falhou: 0x%08x\n", what, (unsigned)rc);
}

// Procura "Nome: valor" no bloco de cabecalhos (sem diferenciar maiusculas).
std::string header_value(const char* block, size_t size, const char* name) {
    std::string all(block, size);
    size_t nlen = strlen(name);
    size_t pos = 0;
    while (pos < all.size()) {
        size_t e = all.find('\n', pos);
        if (e == std::string::npos) e = all.size();
        std::string line = all.substr(pos, e - pos);
        pos = e + 1;
        if (!line.empty() && line[line.size() - 1] == '\r') line.erase(line.size() - 1);
        if (line.size() > nlen && line[nlen] == ':' && strncasecmp(line.c_str(), name, nlen) == 0) {
            size_t v = nlen + 1;
            while (v < line.size() && (line[v] == ' ' || line[v] == '\t')) ++v;
            return line.substr(v);
        }
    }
    return std::string();
}

class SceUpstream : public Upstream {
public:
    SceUpstream() : conn_(-1), req_(-1) {}
    ~SceUpstream() { close(); }

    bool open(const std::string& url, const std::string& range, UpstreamHead& head) {
        close();

        conn_ = sceHttpCreateConnectionWithURL(g_tmpl, url.c_str(), 0);
        if (conn_ < 0) {
            log_err("sceHttpCreateConnectionWithURL", conn_);
            conn_ = -1;
            return false;
        }
        req_ = sceHttpCreateRequestWithURL(conn_, SCE_HTTP_METHOD_GET, url.c_str(), 0);
        if (req_ < 0) {
            log_err("sceHttpCreateRequestWithURL", req_);
            req_ = -1;
            close();
            return false;
        }

        if (!range.empty()) {
            sceHttpAddRequestHeader(req_, "Range", range.c_str(), SCE_HTTP_HEADER_OVERWRITE);
        }
        sceHttpSetConnectTimeOut(req_, kConnectTimeoutUs);
        sceHttpSetSendTimeOut(req_, kIoTimeoutUs);
        sceHttpSetRecvTimeOut(req_, kIoTimeoutUs);

        int rc = sceHttpSendRequest(req_, NULL, 0);
        if (rc < 0) {
            log_err("sceHttpSendRequest", rc);
            close();
            return false;
        }

        int status = 0;
        rc = sceHttpGetStatusCode(req_, &status);
        if (rc < 0) {
            log_err("sceHttpGetStatusCode", rc);
            close();
            return false;
        }
        head.status = status;

        int result = 0;
        uint64_t len = 0;
        rc = sceHttpGetResponseContentLength(req_, &result, &len);
        head.content_length = (rc >= 0 && result == SCE_HTTP_CONTENTLEN_EXIST) ? (long long)len : -1;

        char* hdr = NULL;
        size_t hsz = 0;
        if (sceHttpGetAllResponseHeaders(req_, &hdr, &hsz) >= 0 && hdr != NULL) {
            head.content_type = header_value(hdr, hsz, "Content-Type");
            head.content_range = header_value(hdr, hsz, "Content-Range");
        }
        return true;
    }

    long read(void* buf, size_t len) {
        if (req_ < 0) return -1;
        return (long)sceHttpReadData(req_, buf, len);  // 0 = fim, <0 = erro
    }

    void close() {
        if (req_ >= 0) sceHttpDeleteRequest(req_);
        if (conn_ >= 0) sceHttpDeleteConnection(conn_);
        req_ = -1;
        conn_ = -1;
    }

private:
    int conn_;
    int req_;
};

}  // namespace

bool upstream_init() {
    if (g_http_ctx >= 0) return true;

    g_net_pool = sceNetPoolCreate("gtproxy", (int)kNetPoolSize, 0);
    if (g_net_pool < 0) {
        log_err("sceNetPoolCreate", g_net_pool);
        return false;
    }
    g_ssl_ctx = sceSslInit(kSslPoolSize);
    if (g_ssl_ctx < 0) {
        log_err("sceSslInit", g_ssl_ctx);
        sceNetPoolDestroy(g_net_pool);
        g_net_pool = -1;
        return false;
    }
    g_http_ctx = sceHttpInit(g_net_pool, g_ssl_ctx, kHttpPoolSize);
    if (g_http_ctx < 0) {
        log_err("sceHttpInit", g_http_ctx);
        sceSslTerm(g_ssl_ctx);
        sceNetPoolDestroy(g_net_pool);
        g_http_ctx = g_ssl_ctx = g_net_pool = -1;
        return false;
    }
    g_tmpl = sceHttpCreateTemplate(g_http_ctx, "GTSTORE/1.0", SCE_HTTP_VERSION_1_1, 1);
    if (g_tmpl < 0) {
        log_err("sceHttpCreateTemplate", g_tmpl);
        upstream_term();
        return false;
    }
    sceHttpSetAutoRedirect(g_tmpl, 1);
    return true;
}

void upstream_term() {
    if (g_tmpl >= 0) sceHttpDeleteTemplate(g_tmpl);
    if (g_http_ctx >= 0) sceHttpTerm(g_http_ctx);
    if (g_ssl_ctx >= 0) sceSslTerm(g_ssl_ctx);
    if (g_net_pool >= 0) sceNetPoolDestroy(g_net_pool);
    g_tmpl = g_http_ctx = g_ssl_ctx = g_net_pool = -1;
}

Upstream* upstream_create() { return new SceUpstream(); }
