#ifndef CONFIG_H
#define CONFIG_H

typedef struct {
    char ruta_log[128];
    char ruta_usuarios[128];
    char ruta_db[128];
    char admin_user[32];
    char admin_pass[32];
} Config;

extern Config g_config;

int cargar_config(const char *ruta);

#endif