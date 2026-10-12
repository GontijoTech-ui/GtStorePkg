// Proxy HTTP local (somente 127.0.0.1) para o instalador de pacotes do PS4.
#pragma once

#include <time.h> // Correcao do bug do nanosleep
#include <string>

namespace gtproxy {

bool start(unsigned short port);
void stop();
unsigned short port();

std::string register_url(const std::string& upstream_url, const std::string& file_name = "");
void unregister_url(const std::string& local_url);

}  // namespace gtproxy
