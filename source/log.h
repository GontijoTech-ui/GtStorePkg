// Log simples: grava em arquivo (para ver o que aconteceu no console) e em stderr.
// Tenta /data e /mnt/usb0; se nenhum for gravavel, so stderr.
#pragma once
#include <stdarg.h>
#include <stdio.h>

inline void gt_log(const char* fmt, ...) {
    static const char* const paths[] = {"/data/gtproxy.log", "/mnt/usb0/gtproxy.log"};
    for (unsigned i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
        FILE* f = fopen(paths[i], "a");
        if (!f) continue;
        va_list ap;
        va_start(ap, fmt);
        vfprintf(f, fmt, ap);
        va_end(ap);
        fclose(f);
    }
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}
