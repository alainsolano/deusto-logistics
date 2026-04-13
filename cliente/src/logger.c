#include "logger.h"
#include <stdio.h>
#include <time.h>

void escribir_log(const char* mensaje) {
    FILE *f = fopen("logs/log.txt", "a");
    if (f == NULL) {
        printf("  [!] Error al abrir el archivo de log\n");
        return;
    }

    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    fprintf(f, "[%04d-%02d-%02d %02d:%02d:%02d] %s\n",
        t->tm_year + 1900,
        t->tm_mon  + 1,
        t->tm_mday,
        t->tm_hour,
        t->tm_min,
        t->tm_sec,
        mensaje
    );

    fclose(f);
}