#ifndef DB_H
#define DB_H

int db_inicializar(void);
int db_insertar_movimiento(const char *id_producto, int cantidad, char tipo_op, const char *id_operario);
int db_obtener_stock(const char *id_producto, int *stock, int *entradas, int *salidas);

#endif