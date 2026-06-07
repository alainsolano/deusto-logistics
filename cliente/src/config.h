#ifndef CONFIG_H
#define CONFIG_H

typedef struct {
    char ruta_log[128];        /* log local del cliente  */
    char host_servidor[64];    /* IP o host del servidor */
    int  puerto_servidor;      /* puerto TCP del servidor */
} Config;

extern Config g_config;

int cargar_config(const char *ruta);

#endif
