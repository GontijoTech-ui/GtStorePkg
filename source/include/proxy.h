// Proxy HTTP local (somente 127.0.0.1) para o instalador de pacotes do PS4.
//
// O app registra a URL com token e recebe uma URL curta e sem query string:
//   std::string local = gtproxy::register_url(signed_url, "game.pkg");
//   // -> http://127.0.0.1:8080/d/9f3a1c2e5b7d4a60/game.pkg
// O instalador baixa dessa URL; o proxy repassa para a URL real (com Range).
#pragma once

#include <string>

namespace gtproxy {

// Inicia o servidor numa thread propria. port == 0 escolhe uma porta livre.
// Retorna false se o upstream, o bind ou o listen falharem.
bool start(unsigned short port);

// Para o servidor e espera as conexoes ativas terminarem (ate ~5 s).
void stop();

// Porta em uso (0 se parado).
unsigned short port();

// Registra a URL real e devolve a URL local. `file_name` e opcional, so aparece no
// caminho e e filtrado para [A-Za-z0-9._-]. Devolve "" se o proxy nao estiver ativo.
// Guarda no maximo 64 registros; os mais antigos sao descartados.
std::string register_url(const std::string& upstream_url, const std::string& file_name = "");

void unregister_url(const std::string& local_url);

}  // namespace gtproxy
