#pragma once

#include "../../compartido/protocolo.h"

bool db_init(const char* ruta);

bool db_registrar_movimiento(const MovimientoStock& mov, int& stock_resultante);

int db_obtener_stock(const char* id_producto);

void db_close();


