#include "config.h"
#include "../../compartido/protocolo.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

Config g_config = {
    "logs/log.txt",
    HOST_SERVIDOR,
    PUERTO_SERVIDOR
};

/* Quita el salto de linea final */
static void quitar_salto(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
        s[--n] = '\0';
    }
}

int cargar_config(const char *ruta) {
    FILE *f = fopen(ruta, "r");
    char linea[256];

    if (f == NULL) {
        return -1;
    }

    while (fgets(linea, sizeof(linea), f)) {
        quitar_salto(linea);

        if (strncmp(linea, "RUTA_LOG=", 9) == 0) {
            strncpy(g_config.ruta_log, linea + 9, sizeof(g_config.ruta_log) - 1);
            g_config.ruta_log[sizeof(g_config.ruta_log) - 1] = '\0';
        } else if (strncmp(linea, "HOST_SERVIDOR=", 14) == 0) {
            strncpy(g_config.host_servidor, linea + 14, sizeof(g_config.host_servidor) - 1);
            g_config.host_servidor[sizeof(g_config.host_servidor) - 1] = '\0';
        } else if (strncmp(linea, "PUERTO_SERVIDOR=", 16) == 0) {
            int p = atoi(linea + 16);
            if (p > 0 && p < 65536) {
                g_config.puerto_servidor = p;
            }
        }
    }

    fclose(f);
    return 0;
}
