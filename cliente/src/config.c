#include "config.h"
#include <stdio.h>
#include <string.h>

Config g_config = {
    "logs/log.txt",
    "datos/usuarios.dat",
    "datos/deusto_logistics.db",
    "admin",
    "admin"
};

static void quitar_salto(char *s) {
    size_t n = strlen(s);
    if (n > 0 && s[n - 1] == '\n') {
        s[n - 1] = '\0';
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
        } else if (strncmp(linea, "RUTA_USUARIOS=", 14) == 0) {
            strncpy(g_config.ruta_usuarios, linea + 14, sizeof(g_config.ruta_usuarios) - 1);
            g_config.ruta_usuarios[sizeof(g_config.ruta_usuarios) - 1] = '\0';
        } else if (strncmp(linea, "RUTA_DB=", 8) == 0) {
            strncpy(g_config.ruta_db, linea + 8, sizeof(g_config.ruta_db) - 1);
            g_config.ruta_db[sizeof(g_config.ruta_db) - 1] = '\0';
        } else if (strncmp(linea, "ADMIN_USER=", 11) == 0) {
            strncpy(g_config.admin_user, linea + 11, sizeof(g_config.admin_user) - 1);
            g_config.admin_user[sizeof(g_config.admin_user) - 1] = '\0';
        } else if (strncmp(linea, "ADMIN_PASS=", 11) == 0) {
            strncpy(g_config.admin_pass, linea + 11, sizeof(g_config.admin_pass) - 1);
            g_config.admin_pass[sizeof(g_config.admin_pass) - 1] = '\0';
        }
    }

    fclose(f);
    return 0;
}