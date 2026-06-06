#include "logger.h"
#include "config.h"
#include <stdio.h>
#include <time.h>


static void obtener_timestamp(char* buf, int tam) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    snprintf(buf, tam, "%04d-%02d-%02d %02d:%02d:%02d",
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec);
}

static FILE* abrir_log(void) {
    FILE* f = fopen(g_config.ruta_log, "a");
    if (f == NULL)
        printf("  [!] Error al abrir el archivo de log\n");
    return f;
}


void escribir_log(const char* mensaje) {
    FILE* f = abrir_log();
    if (!f) return;

    char ts[20];
    obtener_timestamp(ts, sizeof(ts));
    fprintf(f, "[%s] %s\n", ts, mensaje);
    fclose(f);
}

void log_login(const char* usuario, int exito) {
    FILE* f = abrir_log();
    if (!f) return;

    char ts[20];
    obtener_timestamp(ts, sizeof(ts));
    fprintf(f, "[%s] LOGIN | usuario: %s | resultado: %s\n",
            ts, usuario, exito ? "OK" : "FALLIDO");
    fclose(f);
}

void log_movimiento(const char* id_producto, char tipo_op,
                    int cantidad, int stock_resultante,
                    const char* id_operario) {
    FILE* f = abrir_log();
    if (!f) return;

    char ts[20];
    obtener_timestamp(ts, sizeof(ts));
    const char* tipo_str = (tipo_op == 'E') ? "ENTRADA" : "SALIDA";
    fprintf(f, "[%s] %s | producto: %s | cantidad: %d"
               " | stock_resultante: %d | operario: %s\n",
            ts, tipo_str, id_producto, cantidad,
            stock_resultante, id_operario);
    fclose(f);
}

void log_alerta_stock(const char* id_producto, int stock_actual) {
    FILE* f = abrir_log();
    if (!f) return;

    char ts[20];
    obtener_timestamp(ts, sizeof(ts));
    fprintf(f, "[%s] ALERTA_STOCK | producto: %s | stock: %d"
               " | por debajo del minimo\n",
            ts, id_producto, stock_actual);
    fclose(f);
}

void log_linea_archivo(int num_linea, const char* resultado) {
    FILE* f = abrir_log();
    if (!f) return;

    char ts[20];
    obtener_timestamp(ts, sizeof(ts));
    fprintf(f, "[%s] ARCHIVO | linea: %d | %s\n",
            ts, num_linea, resultado);
    fclose(f);
}

void log_error_conexion(const char* detalle) {
    FILE* f = abrir_log();
    if (!f) return;

    char ts[20];
    obtener_timestamp(ts, sizeof(ts));
    fprintf(f, "[%s] ERROR_CONEXION | %s\n", ts, detalle);
    fclose(f);
}
