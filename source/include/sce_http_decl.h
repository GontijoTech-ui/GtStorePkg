// Prototipos minimos de libSceNet/libSceSsl/libSceHttp usados por upstream_sce.cpp.
//
// ATENCAO: escritos de memoria da API publica do SDK do PS4 e NAO verificados no
// console. Confira assinaturas e constantes contra os headers do seu SDK (se o
// OpenOrbis tiver orbis/Http.h, prefira incluir o header oficial no lugar deste).
#pragma once

#include <stddef.h>
#include <stdint.h>

extern "C" {
int sceNetPoolCreate(const char* name, int size, int flags);
int sceNetPoolDestroy(int memid);

int sceSslInit(size_t poolSize);
int sceSslTerm(int libsslCtxId);

int sceHttpInit(int libnetMemId, int libsslCtxId, size_t poolSize);
int sceHttpTerm(int libhttpCtxId);
int sceHttpCreateTemplate(int libhttpCtxId, const char* userAgent, int httpVer, int isAutoProxyConf);
int sceHttpDeleteTemplate(int tmplId);
int sceHttpSetAutoRedirect(int id, int enable);
int sceHttpCreateConnectionWithURL(int tmplId, const char* url, int enableKeepalive);
int sceHttpDeleteConnection(int connId);
int sceHttpCreateRequestWithURL(int connId, int method, const char* url, uint64_t contentLength);
int sceHttpDeleteRequest(int reqId);
int sceHttpAddRequestHeader(int id, const char* name, const char* value, uint32_t mode);
int sceHttpSetConnectTimeOut(int id, uint32_t usec);
int sceHttpSetSendTimeOut(int id, uint32_t usec);
int sceHttpSetRecvTimeOut(int id, uint32_t usec);
int sceHttpSendRequest(int reqId, const void* postData, size_t size);
int sceHttpGetStatusCode(int reqId, int* statusCode);
int sceHttpGetResponseContentLength(int reqId, int* result, uint64_t* contentLength);
int sceHttpGetAllResponseHeaders(int reqId, char** header, size_t* headerSize);
int sceHttpReadData(int reqId, void* data, size_t size);
}

enum {
    SCE_HTTP_VERSION_1_1 = 2,
    SCE_HTTP_METHOD_GET = 0,
    SCE_HTTP_HEADER_OVERWRITE = 0,
    SCE_HTTP_CONTENTLEN_EXIST = 0
};
