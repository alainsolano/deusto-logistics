#ifndef LOGGER_H
#define LOGGER_H


void escribir_log(const char* mensaje);

void log_login(const char* usuario, int exito);

void log_movimiento(const char* id_producto, char tipo_op,
                    int cantidad, int stock_resultante,
                    const char* id_operario);

void log_alerta_stock(const char* id_producto, int stock_actual);

void log_linea_archivo(int num_linea, const char* resultado);

void log_error_conexion(const char* detalle);

#endif 
